#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// kernelvec.S内のkerneltrap()を呼び出す。
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// カーネル内で例外やトラップを受け取る準備を行う。
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

// ユーザ空間からカーネルに対する割り込み、例外、またはシステムコールを処理する。
// trampoline.Sから呼び出される。
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // 割り込みと例外をkerneltrap()に送るために設定する。
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();

  // ユーザプログラムカウンタを保存する。
  p->trapframe->epc = r_sepc();

  if(r_scause() == 8){
    // システムコール
    if(killed(p))
      exit(-1);

    // sepcはecall命令を指しているが、次の命令に戻るためにインクリメントする。
    p->trapframe->epc += 4;

    // 割り込みがsepc、scause、sstatusを変更するので、それらのレジスタの操作が終わった後にのみ有効にする。
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // 正常処理
  } else {
    printf("usertrap(): unexpected scause 0x%lx pid=%d\n", r_scause(), p->pid);
    printf("            sepc=0x%lx stval=0x%lx\n", r_sepc(), r_stval());
    setkilled(p);
  }

  if(killed(p))
    exit(-1);

  // タイマー割り込みであればCPUを譲る。
  if(which_dev == 2)
    yield();

  usertrapret();
}

// ユーザ空間に戻るための処理
void
usertrapret(void)
{
  struct proc *p = myproc();

  // kerneltrap()からusertrap()にトラップの送信先を変更する直前なので、ユーザ空間に戻るまで割り込みをオフにする。
  intr_off();

  // システムコール、割り込み、例外をtrampoline.S内のuservecに送るよう設定する。
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  // 次回プロセスがカーネルにトラップする際にuservecが必要とするtrapframeの値を設定する。
  p->trapframe->kernel_satp = r_satp();         // カーネルページテーブル
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // プロセスのカーネルスタック
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // ハートID (CPU ID)

  // trampoline.Sのsretがユーザ空間に戻る際に使用するレジスタを設定する。

  // S Previous Privilegeモードをユーザに設定する。
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // SPPをクリアしてユーザモードに設定
  x |= SSTATUS_SPIE; // ユーザモードでの割り込みを有効にする
  w_sstatus(x);

  // 保存されたユーザプログラムカウンタにS Exception Program Counterを設定する。
  w_sepc(p->trapframe->epc);

  // ユーザページテーブルに切り替えるようにtrampoline.Sに伝える。
  uint64 satp = MAKE_SATP(p->pagetable);

  // trampoline.Sのuserretにジャンプし、ユーザページテーブルに切り替え、ユーザレジスタを復元し、sretでユーザモードに戻る。
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

// kernelvec経由でカーネルコードからの割り込みと例外を処理する。
// 現在のカーネルスタックを使用する。
void
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();

  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    // 不明なソースからの割り込みやトラップ
    printf("scause=0x%lx sepc=0x%lx stval=0x%lx\n", scause, r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // タイマー割り込みであればCPUを譲る。
  if(which_dev == 2 && myproc() != 0)
    yield();

  // yield()がトラップを発生させる可能性があるので、kernelvec.Sのsepc命令で使用するためにトラップレジスタを復元する。
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  if(cpuid() == 0){
    acquire(&tickslock);
    ticks++;
    wakeup(&ticks);
    release(&tickslock);
  }

  // 次のタイマー割り込みを要求する。これにより、割り込み要求もクリアされる。1000000は約0.1秒。
  w_stimecmp(r_time() + 1000000);
}

// 外部割り込みまたはソフトウェア割り込みかどうかを確認し、処理する。
// タイマー割り込みなら2、その他のデバイス割り込みなら1、認識されなかった場合は0を返す。
int
devintr()
{
  uint64 scause = r_scause();

  if(scause == 0x8000000000000009L){
    // これはPLIC経由のスーパーバイザ外部割り込みです。

    // irqはどのデバイスが割り込みを発生させたかを示します。
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // PLICは各デバイスが一度に一つの割り込みしか発生させないようにしているので、デバイスが再び割り込みを発生させることができるようにする。
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000005L){
    // タイマー割り込み
    clockintr();
    return 2;
  } else {
    return 0;
  }
}
