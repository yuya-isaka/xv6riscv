// ミューテックススピンロック。

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"

// スピンロックの初期化
void
initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
}

// ロックを取得する。
// ロックが取得されるまでループ（スピン）する。
void
acquire(struct spinlock *lk)
{
  push_off(); // デッドロックを避けるために割り込みを無効化
  if(holding(lk))
    panic("acquire");

  // RISC-Vでは、sync_lock_test_and_setはアトミックスワップに変換される:
  //   a5 = 1
  //   s1 = &lk->locked
  //   amoswap.w.aq a5, a5, (s1)
  while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
    ;

  // このポイントを超えてロードやストアを移動しないように
  // Cコンパイラおよびプロセッサに通知し、クリティカルセクションの
  // メモリ参照がロック取得後に厳密に行われるようにする。
  // RISC-Vでは、これによりフェンス命令が発行される。
  __sync_synchronize();

  // ロック取得に関する情報を記録する。holding()およびデバッグ用。
  lk->cpu = mycpu();
}

// ロックを解放する。
void
release(struct spinlock *lk)
{
  if(!holding(lk))
    panic("release");

  lk->cpu = 0;

  // クリティカルセクション内のすべてのストアが他のCPUに対して
  // ロックが解放される前に見えるようにし、かつ
  // クリティカルセクション内のロードがロック解放前に厳密に行われるように
  // CコンパイラおよびCPUに対してロードやストアをこのポイントを超えて
  // 移動しないように通知する。
  // RISC-Vでは、これによりフェンス命令が発行される。
  __sync_synchronize();

  // ロックを解放する。これはlk->locked = 0に相当する。
  // このコードはCの代入文を使用しない。C標準は代入が
  // 複数のストア命令で実装される可能性があると示唆しているため。
  // RISC-Vでは、sync_lock_releaseはアトミックスワップに変換される:
  //   s1 = &lk->locked
  //   amoswap.w zero, zero, (s1)
  __sync_lock_release(&lk->locked);

  pop_off();
}

// このCPUがロックを保持しているかどうかを確認する。
// 割り込みは無効でなければならない。
int
holding(struct spinlock *lk)
{
  int r;
  r = (lk->locked && lk->cpu == mycpu());
  return r;
}

// push_off/pop_offはintr_off()/intr_on()に似ているが、マッチングする:
// 2つのpush_off()を行うには2つのpop_off()が必要である。
// また、最初に割り込みが無効の場合、push_off、pop_offはそれを維持する。

void
push_off(void)
{
  int old = intr_get();

  intr_off();
  if(mycpu()->noff == 0)
    mycpu()->intena = old;
  mycpu()->noff += 1;
}

void
pop_off(void)
{
  struct cpu *c = mycpu();
  if(intr_get())
    panic("pop_off - interruptible");
  if(c->noff < 1)
    panic("pop_off");
  c->noff -= 1;
  if(c->noff == 0 && c->intena)
    intr_on();
}
