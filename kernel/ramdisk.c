//
// qemuの-initrdオプションでロードされるディスクイメージを使用するramdiskの実装
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

// ramdiskの初期化関数。現在は何も行わない。
void
ramdiskinit(void)
{
}

// bufのB_DIRTYフラグが設定されている場合、bufをディスクに書き込み、
// B_DIRTYをクリアし、B_VALIDを設定する。
// そうでなければ、B_VALIDが設定されていない場合、bufをディスクから読み込み、
// B_VALIDを設定する。
void
ramdiskrw(struct buf *b)
{
  // bufのロックが取得されていることを確認する。
  if(!holdingsleep(&b->lock))
    panic("ramdiskrw: buf not locked");

  // すでにB_VALIDが設定され、B_DIRTYがクリアされている場合、何もすることがない。
  if((b->flags & (B_VALID|B_DIRTY)) == B_VALID)
    panic("ramdiskrw: nothing to do");

  // ブロック番号がファイルシステムのサイズを超えていないことを確認する。
  if(b->blockno >= FSSIZE)
    panic("ramdiskrw: blockno too big");

  // ディスクアドレスを計算する。
  uint64 diskaddr = b->blockno * BSIZE;
  // RAMDISKのベースアドレスにディスクアドレスを加算してメモリアドレスを得る。
  char *addr = (char *)RAMDISK + diskaddr;

  if(b->flags & B_DIRTY){
    // 書き込み処理
    memmove(addr, b->data, BSIZE); // bufのデータをディスクアドレスにコピー
    b->flags &= ~B_DIRTY; // B_DIRTYフラグをクリア
  } else {
    // 読み込み処理
    memmove(b->data, addr, BSIZE); // ディスクアドレスのデータをbufにコピー
    b->flags |= B_VALID; // B_VALIDフラグを設定
  }
}
