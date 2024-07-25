// スリープロック

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"

// スリープロックの初期化
void
initsleeplock(struct sleeplock *lk, char *name)
{
  // スピンロックを初期化し、スリープロックの操作を保護
  initlock(&lk->lk, "sleep lock");
  lk->name = name;  // デバッグ用にロックの名前を設定
  lk->locked = 0;   // 初期状態ではロックは保持されていない
  lk->pid = 0;      // 初期状態ではロックを保持するプロセスIDはなし
}

// スリープロックの獲得
void
acquiresleep(struct sleeplock *lk)
{
  // スピンロックを獲得してスリープロックの操作を保護
  acquire(&lk->lk);
  // ロックが解放されるまで待機
  while (lk->locked) {
    sleep(lk, &lk->lk);
  }
  // スリープロックを獲得し、現在のプロセスIDを設定
  lk->locked = 1;
  lk->pid = myproc()->pid;
  // スピンロックを解放
  release(&lk->lk);
}

// スリープロックの解放
void
releasesleep(struct sleeplock *lk)
{
  // スピンロックを獲得してスリープロックの操作を保護
  acquire(&lk->lk);
  // ロックを解放し、プロセスIDをクリア
  lk->locked = 0;
  lk->pid = 0;
  // ロックを待っているプロセスを起こす
  wakeup(lk);
  // スピンロックを解放
  release(&lk->lk);
}

// 現在のプロセスがスリープロックを保持しているか確認
int
holdingsleep(struct sleeplock *lk)
{
  int r;

  // スピンロックを獲得してスリープロックの操作を保護
  acquire(&lk->lk);
  // ロックが保持されており、現在のプロセスが保持者であるか確認
  r = lk->locked && (lk->pid == myproc()->pid);
  // スピンロックを解放
  release(&lk->lk);
  return r;
}
