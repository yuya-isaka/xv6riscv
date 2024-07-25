// ユーザープロセス、カーネルスタック、ページテーブルページ、
// パイプバッファのための物理メモリアロケータ。
// 4096バイトのページ全体を割り当てる。

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // カーネルの終了アドレス。
// kernel.ldで定義されている。

struct run {
  struct run *next; // 次の空きページを指すポインタ。
};

struct {
  struct spinlock lock; // 物理メモリの割り当て/解放を保護するスピンロック。
  struct run *freelist; // 空きページのリスト。
} kmem;

// 物理メモリアロケータを初期化する関数である。
void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

// 指定された範囲の物理メモリを解放する関数である。
void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start); // 開始アドレスをページ境界に切り上げる。
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p); // 各ページを解放する。
}

// 物理メモリのページを解放する関数である。
// 通常、kalloc()の呼び出しによって返されたポインタを引数とする。
// （例外は、アロケータの初期化時である。上記のkinitを参照。）
void
kfree(void *pa)
{
  struct run *r;

  // paがページアラインされているか、カーネル終了アドレスより小さいか、
  // 物理メモリの範囲外でないかをチェックする。
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // ダングリング参照を捕まえるためにジャンクで埋める。
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist; // 解放リストの先頭に追加する。
  kmem.freelist = r;
  release(&kmem.lock);
}

// 4096バイトの物理メモリのページを1つ割り当てる関数である。
// カーネルが使用できるポインタを返す。
// メモリが割り当てられない場合は0を返す。
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist; // 解放リストの先頭からページを取得する。
  if(r)
    kmem.freelist = r->next; // リストを更新する。
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // ジャンクで埋める。
  return (void*)r;
}
