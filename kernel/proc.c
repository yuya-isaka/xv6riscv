#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// 親プロセスがwait()中にウェイクアップが失われないようにするためのロック。
// p->parentを使用する際のメモリモデルを遵守するために使用する。
// p->lockを取得する前に必ず取得する必要がある。
struct spinlock wait_lock;

// 各プロセスのカーネルスタック用にページを割り当てる。
// メモリの高い位置にマップし、無効なガードページが続く。
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// プロセステーブルを初期化する関数である。
void
procinit(void)
{
  struct proc *p;

  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->state = UNUSED;
      p->kstack = KSTACK((int) (p - proc));
  }
}

// 割り込みを無効にして呼び出す必要がある。
// プロセスが別のCPUに移動する競合を防ぐためである。
int
cpuid()
{
  int id = r_tp();
  return id;
}

// 現在のCPUの構造体を返す。
// 割り込みは無効にする必要がある。
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// 現在のプロセス構造体を返す。プロセスがない場合は0を返す。
struct proc*
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

// 新しいPIDを割り当てる関数である。
int
allocpid()
{
  int pid;

  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// UNUSED状態のプロセスをプロセステーブルで探す。
// 見つかった場合、カーネルで動作するために必要な状態を初期化し、
// p->lockを保持した状態で返す。
// 空きプロセスがない場合やメモリ割り当てに失敗した場合は0を返す。
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // トラップフレームページを割り当てる。
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // 空のユーザーページテーブル。
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // 新しいコンテキストをセットアップして、forkretで実行を開始する。
  // forkretはユーザースペースに戻る。
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// プロセス構造体とそれに関連するデータを解放する。
// ユーザーページを含む。
// p->lockは保持する必要がある。
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

// 指定されたプロセスのためにユーザーページテーブルを作成する。
// ユーザーメモリは含まれないが、トランポリンとトラップフレームページは含まれる。
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // 空のページテーブル。
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // トランポリンコードを最高のユーザ仮想アドレスにマップする。
  // スーパーバイザーのみが使用し、ユーザースペースに行き来する際に使用されるため、PTE_Uは不要。
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // トランポリンページの直下にトラップフレームページをマップする。
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// プロセスのページテーブルを解放し、それが参照する物理メモリを解放する。
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// exec("/init")を呼び出すユーザープログラム。
// ../user/initcode.Sからアセンブルされる。
// od -t xC ../user/initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// 最初のユーザープロセスをセットアップする関数である。
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;

  // 1つのユーザーページを割り当て、initcodeの命令とデータをコピーする。
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // カーネルからユーザーへの初回の「リターン」の準備をする。
  p->trapframe->epc = 0;      // ユーザープログラムカウンタ
  p->trapframe->sp = PGSIZE;  // ユーザースタックポインタ

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// ユーザーメモリをnバイトだけ増減させる。
// 成功時は0を返し、失敗時は-1を返す。
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// 新しいプロセスを作成し、親プロセスをコピーする。
// 子プロセスのカーネルスタックをセットアップして、fork()システムコールから返るようにする。
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // プロセスを割り当てる。
  if((np = allocproc()) == 0){
    return -1;
  }

  // 親プロセスから子プロセスへユーザーメモリをコピーする。
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // 保存されたユーザーレジスタをコピーする。
  *(np->trapframe) = *(p->trapframe);

  // 子プロセスではforkは0を返すようにする。
  np->trapframe->a0 = 0;

  // オープンしているファイルディスクリプタの参照カウントを増やす。
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}

// pの孤立した子プロセスをinitに移譲する。
// 呼び出し元はwait_lockを保持している必要がある。
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// 現在のプロセスを終了させる関数である。戻り値はない。
// 終了したプロセスは、その親プロセスがwait()を呼び出すまでゾンビ状態で残る。
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // オープンしているすべてのファイルを閉じる。
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // すべての子プロセスをinitに移譲する。
  reparent(p);

  // 親プロセスがwait()でスリープしている可能性がある。
  wakeup(p->parent);

  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // スケジューラにジャンプし、二度と戻らない。
  sched();
  panic("zombie exit");
}

// 子プロセスが終了するのを待ち、そのpidを返す。
// このプロセスに子プロセスがいない場合は-1を返す。
int
wait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // テーブルをスキャンして終了した子プロセスを探す。
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        // 子プロセスがまだexit()またはswtch()中でないことを確認する。
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          // 見つかった。
          pid = pp->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // 子プロセスがいない場合、待機する意味はない。
    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;
    }

    // 子プロセスが終了するのを待つ。
    sleep(p, &wait_lock);  // DOC: wait-sleep
  }
}

// CPUごとのプロセススケジューラ。
// 各CPUは自身を設定した後、scheduler()を呼び出す。
// スケジューラは戻らない。以下のことを繰り返す:
//  - 実行するプロセスを選ぶ。
//  - 選ばれたプロセスを実行する。
//  - 最終的にプロセスが制御をスケジューラに戻す。
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for(;;){
    // 最後に実行されたプロセスが割り込みを無効にしている可能性がある。
    // すべてのプロセスが待機している場合のデッドロックを避けるために有効にする。
    intr_on();

    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // 選ばれたプロセスにスイッチする。
        // プロセスの仕事は、ロックを解放し、
        // 再びここに戻る前にロックを再取得することである。
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // プロセスの実行が終了。
        // プロセスがここに戻る前にp->stateを変更する必要がある。
        c->proc = 0;
      }
      release(&p->lock);
    }
  }
}

// スケジューラにスイッチする。
// p->lockのみを保持し、proc->stateを変更している必要がある。
// intenaを保存および復元する。
// intenaはこのカーネルスレッドのプロパティであり、このCPUのプロパティではないためである。
// これをproc->intenaおよびproc->noffにすると、
// ロックが保持されているがプロセスがない場所で壊れる。
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// 1回のスケジューリングラウンドの間、CPUを放棄する。
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// フォークされた子プロセスがスケジューラによって初めてスケジュールされる際に
// forkretにスイッチする。
void
forkret(void)
{
  static int first = 1;

  // スケジューラからのp->lockをまだ保持している。
  release(&myproc()->lock);

  if (first) {
    // ファイルシステムの初期化は通常のプロセスのコンテキストで実行する必要がある。
    // （例えば、sleepを呼び出すため）、main()から直接呼び出すことはできない。
    fsinit(ROOTDEV);

    first = 0;
    // 他のコアがfirst=0を認識するようにする。
    __sync_synchronize();
  }

  usertrapret();
}

// ロックをアトミックに解放して、チャネル上でスリープする。
// 起床時にロックを再取得する。
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  // p->stateを変更し、schedを呼び出すためにp->lockを取得する必要がある。
  // p->lockを保持することで、wakeupの競合を防ぐことができる。
  // よってlkを解放しても問題ない。

  acquire(&p->lock);  // DOC: sleeplock1
  release(lk);

  // スリープ状態に入る。
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // 後片付け。
  p->chan = 0;

  // 元のロックを再取得する。
  release(&p->lock);
  acquire(lk);
}

// チャネル上でスリープしているすべてのプロセスをウェイクアップする。
// p->lockなしで呼び出す必要がある。
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}

// 指定されたpidのプロセスを終了させる。
// 被害者はユーザースペースに戻ろうとするまで終了しない
// （trap.cのusertrap()を参照）。
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // sleep()からプロセスを起こす。
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

// プロセスを終了状態にする関数である。
void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

// プロセスが終了状態かどうかをチェックする関数である。
int
killed(struct proc *p)
{
  int k;

  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// ユーザーアドレスまたはカーネルアドレスにコピーする。
// usr_dstに応じてどちらにコピーするか決まる。
// 成功時には0を、エラー時には-1を返す。
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// ユーザーアドレスまたはカーネルアドレスからコピーする。
// usr_srcに応じてどちらからコピーするか決まる。
// 成功時には0を、エラー時には-1を返す。
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// プロセスのリストをコンソールに出力する。デバッグ用。
// ユーザーがコンソールで^Pを入力すると実行される。
// ロックなしで実行して、機械のスタックがさらに固まるのを避ける。
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}
