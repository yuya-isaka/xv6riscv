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

// 割り込みの状態を制御し、特定のコードが実行中に割り込みが発生しないようにするための関数
// 割り込みのネストされた無効化/有効化を管理するために設計

// push_offは割り込みを無効化し、その状態を保存
// 複数回呼び出された場合でも対応できるように、無効化の回数をカウント
void
push_off(void)
{
  int old = intr_get(); // 現在の割り込み状態を保存

  intr_off(); // 割り込みを無効化

  // 無効化する前、割り込みが有効だった場合
  if(mycpu()->noff == 0) // 現在のCPUの割り込みが無効化された回数が0の場合
    mycpu()->warikomi = old; // 以前の割り込み状態を保存

  mycpu()->noff += 1; // 割り込みが無効化された回数をインクリメント
}

// pop_offは割り込みが無効化されていることを確認し、その状態をもとに戻す
// 複数回無効化されている場合は、カウントを減らし、最後に無効化が解除されるタイミングで割り込みを有効化
void
pop_off(void)
{
  struct cpu *c = mycpu();
  if(intr_get()) // 割り込みが有効な場合
    panic("pop_off - interruptible"); // パニックを起こす
  if(c->noff < 1) // 割り込みが無効化された回数が1未満の場合
    panic("pop_off"); // パニックを起こす
  c->noff -= 1; // 割り込みが無効化された回数をデクリメント
  if(c->noff == 0 && c->warikomi) // noffが0になり、以前の割り込み状態が有効な場合
    intr_on(); // 割り込みを有効化
}
