#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

// シンプルなログ機構であり、複数のFSシステムコールの同時実行を許可する。
//
// ログトランザクションは複数のFSシステムコールの更新を含む。
// ログシステムは、アクティブなFSシステムコールがない場合にのみコミットする。
// したがって、コミットが未コミットのシステムコールの更新をディスクに書き込むかどうか
// を考慮する必要はない。
//
// システムコールはbegin_op()/end_op()を呼び出して開始と終了をマークする必要がある。
// 通常、begin_op()は進行中のFSシステムコールのカウントをインクリメントして戻るだけである。
// しかし、ログがすぐにいっぱいになると判断した場合は、最後のend_op()がコミットするまで
// スリープする。
//
// ログはディスクブロックを含む物理リドゥログである。
// ディスク上のログフォーマット:
//   ヘッダーブロックには、ブロックA、B、Cのブロック番号が含まれる。
//   ブロックA
//   ブロックB
//   ブロックC
//   ...
// ログの追加は同期的である。

// ヘッダーブロックの内容であり、ディスク上のヘッダーブロックと
// コミット前にメモリ内で記録するログブロック番号を追跡するために使用される。
struct logheader {
  int n;
  int block[LOGSIZE];
};

// ログの構造体である。
struct log {
  struct spinlock lock;
  int start;
  int size;
  int outstanding; // 実行中のFSシステムコールの数。
  int committing;  // commit()中であることを示し、待機するよう指示する。
  int dev;
  struct logheader lh;
};
struct log log;

static void recover_from_log(void);
static void commit();

void
initlog(int dev, struct superblock *sb)
{
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  initlock(&log.lock, "log");
  log.start = sb->logstart;
  log.size = sb->nlog;
  log.dev = dev;
  recover_from_log();
}

// コミットされたブロックをログからホームロケーションにコピーする
static void
install_trans(int recovering)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *lbuf = bread(log.dev, log.start+tail+1); // ログブロックを読み込む
    struct buf *dbuf = bread(log.dev, log.lh.block[tail]); // 目的地ブロックを読み込む
    memmove(dbuf->data, lbuf->data, BSIZE);  // ブロックを目的地にコピーする
    bwrite(dbuf);  // 目的地ブロックをディスクに書き込む
    if(recovering == 0)
      bunpin(dbuf);
    brelse(lbuf);
    brelse(dbuf);
  }
}

// ログヘッダーをディスクからメモリ内のログヘッダーに読み込む
static void
read_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *) (buf->data);
  int i;
  log.lh.n = lh->n;
  for (i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// メモリ内のログヘッダーをディスクに書き込む。
// これが現在のトランザクションがコミットされる真のポイントである。
static void
write_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *hb = (struct logheader *) (buf->data);
  int i;
  hb->n = log.lh.n;
  for (i = 0; i < log.lh.n; i++) {
    hb->block[i] = log.lh.block[i];
  }
  bwrite(buf);
  brelse(buf);
}

static void
recover_from_log(void)
{
  read_head();
  install_trans(1); // コミットされている場合、ログからディスクにコピーする
  log.lh.n = 0;
  write_head(); // ログをクリアする
}

// 各FSシステムコールの開始時に呼び出される関数である。
void
begin_op(void)
{
  acquire(&log.lock);
  while(1){
    if(log.committing){
      sleep(&log, &log.lock);
    } else if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGSIZE){
      // この操作がログスペースを使い果たす可能性があるため、コミットを待つ。
      sleep(&log, &log.lock);
    } else {
      log.outstanding += 1;
      release(&log.lock);
      break;
    }
  }
}

// 各FSシステムコールの終了時に呼び出される関数である。
// これが最後の未完了の操作であればコミットする。
void
end_op(void)
{
  int do_commit = 0;

  acquire(&log.lock);
  log.outstanding -= 1;
  if(log.committing)
    panic("log.committing");
  if(log.outstanding == 0){
    do_commit = 1;
    log.committing = 1;
  } else {
    // begin_op()がログスペースを待機している可能性があるため、
    // log.outstandingをデクリメントすると予約済みスペースが減少する。
    wakeup(&log);
  }
  release(&log.lock);

  if(do_commit){
    // ロックを保持せずにコミットを呼び出す。ロックを保持したままスリープすることは許可されていないため。
    commit();
    acquire(&log.lock);
    log.committing = 0;
    wakeup(&log);
    release(&log.lock);
  }
}

// キャッシュからログに修正されたブロックをコピーする。
static void
write_log(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *to = bread(log.dev, log.start+tail+1); // ログブロック
    struct buf *from = bread(log.dev, log.lh.block[tail]); // キャッシュブロック
    memmove(to->data, from->data, BSIZE);
    bwrite(to);  // ログに書き込む
    brelse(from);
    brelse(to);
  }
}

static void
commit()
{
  if (log.lh.n > 0) {
    write_log();     // キャッシュからログに修正されたブロックを書き込む
    write_head();    // ヘッダーをディスクに書き込む - 実際のコミット
    install_trans(0); // 書き込みをホームロケーションにインストールする
    log.lh.n = 0;
    write_head();    // トランザクションをログから消去する
  }
}

// 呼び出し元がb->dataを修正し、バッファの使用を終了したことを示す。
// ブロック番号を記録し、refcntを増加させることでキャッシュにピン留めする。
// commit()/write_log()はディスクへの書き込みを行う。
//
// log_write()はbwrite()の代わりに使用される。典型的な使用例は以下の通り:
//   bp = bread(...)
//   modify bp->data[]
//   log_write(bp)
//   brelse(bp)
void
log_write(struct buf *b)
{
  int i;

  acquire(&log.lock);
  if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)
    panic("too big a transaction");
  if (log.outstanding < 1)
    panic("log_write outside of trans");

  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno)   // ログ吸収
      break;
  }
  log.lh.block[i] = b->blockno;
  if (i == log.lh.n) {  // 新しいブロックをログに追加するか？
    bpin(b);
    log.lh.n++;
  }
  release(&log.lock);
}
