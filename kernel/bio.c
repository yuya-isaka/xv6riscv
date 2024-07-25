// バッファキャッシュである。
//
// バッファキャッシュは、ディスクブロック内容のキャッシュされたコピーを保持する
// buf構造体のリンクリストである。メモリ内にディスクブロックをキャッシュすることで
// ディスク読み取りの回数を減らし、複数のプロセスが使用するディスクブロックの
// 同期ポイントも提供する。
//
// インターフェース:
// * 特定のディスクブロックのバッファを取得するには、breadを呼び出す。
// * バッファデータを変更した後、bwriteを呼び出してディスクに書き込む。
// * バッファの使用が終わったら、brelseを呼び出す。
// * brelseを呼び出した後はバッファを使用しない。
// * 同時に一つのプロセスだけがバッファを使用できるため、必要以上に保持しない。

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // すべてのバッファのリンクリストである。
  // 使用頻度順にソートされている。
  // head.nextが最も最近使用されたバッファであり、head.prevが最も古いバッファである。
  struct buf head;
} bcache;

// バッファキャッシュを初期化する関数である。
void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // バッファのリンクリストを作成する。
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}

// デバイスdev上のブロックをバッファキャッシュ内で検索する関数である。
// 見つからなかった場合、バッファを割り当てる。
// いずれの場合も、ロックされたバッファを返す。
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  acquire(&bcache.lock);

  // ブロックはすでにキャッシュされているか？
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // キャッシュされていない。
  // 最も最近使われていない未使用のバッファをリサイクルする。
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}

// 指定されたブロックの内容を持つロックされたバッファを返す関数である。
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// バッファの内容をディスクに書き込む関数である。ロックされている必要がある。
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// ロックされたバッファを解放する関数である。
// 最も最近使用されたリストの先頭に移動する。
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // 誰も待っていない。
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }

  release(&bcache.lock);
}

// バッファの参照カウントを増加させる関数である。
void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

// バッファの参照カウントを減少させる関数である。
void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}
