// ファイルシステムの実装である。五つの層から成る:
//   + Blocks: 生ディスクブロックのアロケータである。
//   + Log: マルチステップ更新のクラッシュリカバリである。
//   + Files: inodeのアロケータ、読み取り、書き込み、メタデータである。
//   + Directories: 他のinodeのリストを含む特別な内容を持つinodeである。
//   + Names: /usr/rtm/xv6/fs.c のような便利な命名のためのパスである。
//
// このファイルは低レベルのファイルシステム操作ルーチンを含む。
// （高レベルの）システムコールの実装はsysfile.cにある。

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "file.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

// ディスクデバイスごとに一つのスーパーブロックがあるべきであるが、我々は一つのデバイスで動作する。
struct superblock sb;

// スーパーブロックを読み込む関数である。
static void
readsb(int dev, struct superblock *sb)
{
  struct buf *bp;

  bp = bread(dev, 1);
  memmove(sb, bp->data, sizeof(*sb));
  brelse(bp);
}

// ファイルシステムを初期化する関数である。
void
fsinit(int dev) {
  readsb(dev, &sb);
  if(sb.magic != FSMAGIC)
    panic("invalid file system");
  initlog(dev, &sb);
}

// ブロックをゼロクリアする関数である。
static void
bzero(int dev, int bno)
{
  struct buf *bp;

  bp = bread(dev, bno);
  memset(bp->data, 0, BSIZE);
  log_write(bp);
  brelse(bp);
}

// ブロック。

// ゼロクリアされたディスクブロックを割り当てる関数である。
// ディスクスペースがない場合は0を返す。
static uint
balloc(uint dev)
{
  int b, bi, m;
  struct buf *bp;

  bp = 0;
  for(b = 0; b < sb.size; b += BPB){
    bp = bread(dev, BBLOCK(b, sb));
    for(bi = 0; bi < BPB && b + bi < sb.size; bi++){
      m = 1 << (bi % 8);
      if((bp->data[bi/8] & m) == 0){  // ブロックが空いているか？
        bp->data[bi/8] |= m;  // 使用中としてマークする。
        log_write(bp);
        brelse(bp);
        bzero(dev, b + bi);
        return b + bi;
      }
    }
    brelse(bp);
  }
  printf("balloc: out of blocks\n");
  return 0;
}

// ディスクブロックを解放する関数である。
static void
bfree(int dev, uint b)
{
  struct buf *bp;
  int bi, m;

  bp = bread(dev, BBLOCK(b, sb));
  bi = b % BPB;
  m = 1 << (bi % 8);
  if((bp->data[bi/8] & m) == 0)
    panic("freeing free block");
  bp->data[bi/8] &= ~m;
  log_write(bp);
  brelse(bp);
}

// Inodes。

// inodeは一つの名前のないファイルを表す。
// inodeディスク構造はメタデータを保持する：ファイルの種類、サイズ、リンク数、ファイルの内容を保持するブロックのリストである。

// inodeはディスク上のsb.inodestartブロックに順次配置される。
// 各inodeにはその位置を示す番号がある。

// カーネルはメモリ内で使用中のinodeのテーブルを保持し、
// 複数のプロセスが使用するinodeへのアクセスを同期させる場所を提供する。
// メモリ内のinodeには、ディスク上に保存されない管理情報（ip->refおよびip->valid）が含まれる。

// inodeとそのメモリ内の表現は、ファイルシステムコードが使用できるようになる前に一連の状態を経る。

// * 割り当て：inodeが割り当てられているのは、その種類（ディスク上）がゼロでない場合である。
//   ialloc()が割り当て、iput()が参照およびリンクカウントがゼロになった場合に解放する。

// * テーブルでの参照：inodeテーブルのエントリが空いているのはip->refがゼロの場合である。
//   それ以外の場合、ip->refはエントリへのメモリ内ポインタの数を追跡する（オープンファイルおよび現在のディレクトリ）。
//   iget()はテーブルエントリを見つけたり作成したりし、参照カウントをインクリメントし、iput()は参照カウントをデクリメントする。

// * 有効：inodeテーブルエントリ内の情報（種類、サイズなど）はip->validが1の場合にのみ正しい。
//   ilock()はディスクからinodeを読み込み、ip->validをセットし、iput()はip->refがゼロになった場合にip->validをクリアする。

// * ロック：ファイルシステムコードはinodeとその内容の情報を調べたり変更したりする前にinodeをロックする必要がある。

// したがって、典型的なシーケンスは次の通りである：
//   ip = iget(dev, inum)
//   ilock(ip)
//   ... ip->xxx を調べたり変更したりする...
//   iunlock(ip)
//   iput(ip)

// ilock()はiget()とは別であり、システムコールがinodeへの長期的な参照を取得できるようにし（オープンファイルの場合）、短期間（例：read()内）だけロックする。
// この分離は、パス名のルックアップ中のデッドロックや競合を回避するのにも役立つ。iget()はip->refをインクリメントし、inodeがテーブルに留まり、そのポインタが有効なままであることを保証する。

// 多くの内部ファイルシステム関数は、呼び出し元が関与するinodeをロックしていることを期待する。
// これにより、呼び出し元がマルチステップのアトミック操作を作成できるようになる。

// itable.lockスピンロックはitableエントリの割り当てを保護する。
// ip->refがエントリが空いているかどうかを示し、ip->devおよびip->inumがエントリが保持しているinodeを示すため、これらのフィールドを使用する際にはitable.lockを保持する必要がある。

// ip->lockスリープロックはref、dev、およびinum以外のすべてのipフィールドを保護する。
// ip->valid、ip->size、ip->typeなどを読み書きするためにはip->lockを保持する必要がある。

struct {
  struct spinlock lock;
  struct inode inode[NINODE];
} itable;

// ファイルシステムを初期化する関数である。
void
iinit()
{
  int i = 0;

  initlock(&itable.lock, "itable");
  for(i = 0; i < NINODE; i++) {
    initsleeplock(&itable.inode[i].lock, "inode");
  }
}

// デバイスdev上のinodeを取得する関数である。
static struct inode* iget(uint dev, uint inum);

// デバイスdev上のinodeを割り当てる関数である。
// 種類を指定して割り当て済みとしてマークする。
// ロックされていないが割り当て済みで参照されたinodeを返す。
// 空きinodeがない場合はNULLを返す。
struct inode*
ialloc(uint dev, short type)
{
  int inum;
  struct buf *bp;
  struct dinode *dip;

  for(inum = 1; inum < sb.ninodes; inum++){
    bp = bread(dev, IBLOCK(inum, sb));
    dip = (struct dinode*)bp->data + inum%IPB;

    if(dip->type == 0){  // 空いているinodeであるか？
      memset(dip, 0, sizeof(*dip));
      dip->type = type;
      log_write(bp);   // ディスク上に割り当て済みとしてマークする。
      brelse(bp);
      return iget(dev, inum);
    }
    brelse(bp);
  }
  printf("ialloc: no inodes\n");
  return 0;
}

// 修正されたメモリ内のinodeをディスクにコピーする関数である。
// ip->xxxフィールドに変更を加えた後に必ず呼び出す必要がある。
// 呼び出し元はip->lockを保持している必要がある。
void
iupdate(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  bp = bread(ip->dev, IBLOCK(ip->inum, sb));
  dip = (struct dinode*)bp->data + ip->inum%IPB;
  dip->type = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;
  memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
  log_write(bp);
  brelse(bp);
}

// デバイスdev上のinum番号を持つinodeを見つけて
// メモリ内のコピーを返す関数である。
// inodeをロックせず、ディスクから読み込むこともない。
static struct inode*
iget(uint dev, uint inum)
{
  struct inode *ip, *empty;

  acquire(&itable.lock);

  // inodeがすでにテーブルにあるか？
  empty = 0;
  for(ip = &itable.inode[0]; ip < &itable.inode[NINODE]; ip++){
    if(ip->ref > 0 && ip->dev == dev && ip->inum == inum){
      ip->ref++;
      release(&itable.lock);
      return ip;
    }
    if(empty == 0 && ip->ref == 0)    // 空きスロットを覚えておく。
      empty = ip;
  }

  // inodeエントリをリサイクルする。
  if(empty == 0)
    panic("iget: no inodes");

  ip = empty;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  ip->valid = 0;
  release(&itable.lock);

  return ip;
}

// ipの参照カウントを増加させる関数である。
// ipを返し、ip = idup(ip1)のイディオムを可能にする。
struct inode*
idup(struct inode *ip)
{
  acquire(&itable.lock);
  ip->ref++;
  release(&itable.lock);
  return ip;
}

// 指定されたinodeをロックする関数である。
// 必要に応じてディスクからinodeを読み込む。
void
ilock(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  if(ip == 0 || ip->ref < 1)
    panic("ilock");

  acquiresleep(&ip->lock);

  if(ip->valid == 0){
    bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct dinode*)bp->data + ip->inum%IPB;
    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
    brelse(bp);
    ip->valid = 1;
    if(ip->type == 0)
      panic("ilock: no type");
  }
}

// 指定されたinodeのロックを解除する関数である。
void
iunlock(struct inode *ip)
{
  if(ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
    panic("iunlock");

  releasesleep(&ip->lock);
}

// メモリ内inodeへの参照を減らす関数である。
// これが最後の参照である場合、inodeテーブルエントリをリサイクルできる。
// これが最後の参照であり、inodeにリンクがない場合、
// ディスク上のinode（およびその内容）を解放する。
// iput()へのすべての呼び出しはトランザクション内で行う必要がある。
// inodeを解放する可能性があるため。
void
iput(struct inode *ip)
{
  acquire(&itable.lock);

  if(ip->ref == 1 && ip->valid && ip->nlink == 0){
    // inodeにリンクがなく、他の参照もない場合：トランケートして解放する。

    // ip->ref == 1 ということは、他のプロセスがipをロックしていないことを意味し、
    // このacquiresleep()はブロック（またはデッドロック）しない。
    acquiresleep(&ip->lock);

    release(&itable.lock);

    itrunc(ip);
    ip->type = 0;
    iupdate(ip);
    ip->valid = 0;

    releasesleep(&ip->lock);

    acquire(&itable.lock);
  }

  ip->ref--;
  release(&itable.lock);
}

// 共通のイディオムである：ロック解除してからputする。
void
iunlockput(struct inode *ip)
{
  iunlock(ip);
  iput(ip);
}

// inodeの内容。

// 各inodeに関連付けられた内容（データ）は、ディスク上のブロックに格納される。
// 最初のNDIRECTブロック番号はip->addrs[]にリストされる。
// 次のNINDIRECTブロックはip->addrs[NDIRECT]ブロックにリストされる。

// inode ipのnthブロックのディスクブロックアドレスを返す関数である。
// そのようなブロックが存在しない場合、bmapは一つを割り当てる。
// ディスクスペースがない場合は0を返す。
static uint
bmap(struct inode *ip, uint bn)
{
  uint addr, *a;
  struct buf *bp;

  if(bn < NDIRECT){
    if((addr = ip->addrs[bn]) == 0){
      addr = balloc(ip->dev);
      if(addr == 0)
        return 0;
      ip->addrs[bn] = addr;
    }
    return addr;
  }
  bn -= NDIRECT;

  if(bn < NINDIRECT){
    // 間接ブロックをロードし、必要に応じて割り当てる。
    if((addr = ip->addrs[NDIRECT]) == 0){
      addr = balloc(ip->dev);
      if(addr == 0)
        return 0;
      ip->addrs[NDIRECT] = addr;
    }
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn]) == 0){
      addr = balloc(ip->dev);
      if(addr){
        a[bn] = addr;
        log_write(bp);
      }
    }
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}

// inodeをトランケートする関数である（内容を破棄する）。
// 呼び出し元はip->lockを保持している必要がある。
void
itrunc(struct inode *ip)
{
  int i, j;
  struct buf *bp;
  uint *a;

  for(i = 0; i < NDIRECT; i++){
    if(ip->addrs[i]){
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

  if(ip->addrs[NDIRECT]){
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint*)bp->data;
    for(j = 0; j < NINDIRECT; j++){
      if(a[j])
        bfree(ip->dev, a[j]);
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

  ip->size = 0;
  iupdate(ip);
}

// inodeからstat情報をコピーする関数である。
// 呼び出し元はip->lockを保持している必要がある。
void
stati(struct inode *ip, struct stat *st)
{
  st->dev = ip->dev;
  st->ino = ip->inum;
  st->type = ip->type;
  st->nlink = ip->nlink;
  st->size = ip->size;
}

// inodeからデータを読み取る関数である。
// 呼び出し元はip->lockを保持している必要がある。
// user_dst==1の場合、dstはユーザ仮想アドレスである。
// それ以外の場合、dstはカーネルアドレスである。
int
readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if(off > ip->size || off + n < off)
    return 0;
  if(off + n > ip->size)
    n = ip->size - off;

  for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
    uint addr = bmap(ip, off/BSIZE);
    if(addr == 0)
      break;
    bp = bread(ip->dev, addr);
    m = min(n - tot, BSIZE - off%BSIZE);
    if(either_copyout(user_dst, dst, bp->data + (off % BSIZE), m) == -1) {
      brelse(bp);
      tot = -1;
      break;
    }
    brelse(bp);
  }
  return tot;
}

// inodeにデータを書き込む関数である。
// 呼び出し元はip->lockを保持している必要がある。
// user_src==1の場合、srcはユーザ仮想アドレスである。
// それ以外の場合、srcはカーネルアドレスである。
// 成功したバイト数を返す。
// 返された値が要求されたnより少ない場合、何らかのエラーが発生している。
int
writei(struct inode *ip, int user_src, uint64 src, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if(off > ip->size || off + n < off)
    return -1;
  if(off + n > MAXFILE*BSIZE)
    return -1;

  for(tot=0; tot<n; tot+=m, off+=m, src+=m){
    uint addr = bmap(ip, off/BSIZE);
    if(addr == 0)
      break;
    bp = bread(ip->dev, addr);
    m = min(n - tot, BSIZE - off%BSIZE);
    if(either_copyin(bp->data + (off % BSIZE), user_src, src, m) == -1) {
      brelse(bp);
      break;
    }
    log_write(bp);
    brelse(bp);
  }

  if(off > ip->size)
    ip->size = off;

  // ループ中にbmap()が呼ばれ、新しいブロックがip->addrs[]に追加された可能性があるため、
  // サイズが変更されなくてもi-nodeをディスクに書き戻す。
  iupdate(ip);

  return tot;
}

// ディレクトリ

// 文字列sとtを比較する関数である。
int
namecmp(const char *s, const char *t)
{
  return strncmp(s, t, DIRSIZ);
}

// ディレクトリ内のディレクトリエントリを探す関数である。
// 見つかった場合、エントリのバイトオフセットを*poffに設定する。
struct inode*
dirlookup(struct inode *dp, char *name, uint *poff)
{
  uint off, inum;
  struct dirent de;

  if(dp->type != T_DIR)
    panic("dirlookup not DIR");

  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlookup read");
    if(de.inum == 0)
      continue;
    if(namecmp(name, de.name) == 0){
      // エントリがパス要素と一致する。
      if(poff)
        *poff = off;
      inum = de.inum;
      return iget(dp->dev, inum);
    }
  }

  return 0;
}

// 新しいディレクトリエントリ（name, inum）をディレクトリdpに書き込む関数である。
// 成功した場合は0を返し、失敗した場合（例：ディスクブロックがない場合）は-1を返す。
int
dirlink(struct inode *dp, char *name, uint inum)
{
  int off;
  struct dirent de;
  struct inode *ip;

  // 名前が存在しないことを確認する。
  if((ip = dirlookup(dp, name, 0)) != 0){
    iput(ip);
    return -1;
  }

  // 空のディレクトリエントリを探す。
  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if(de.inum == 0)
      break;
  }

  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  if(writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    return -1;

  return 0;
}

// パス

// パスから次のパス要素をコピーする関数である。
// コピーした要素の後続の要素へのポインタを返す。
// 返されたパスには先頭のスラッシュが含まれないため、
// nameが最後の要素であるかを確認するために*path=='\0'をチェックできる。
// 名前を取り除く要素がない場合、0を返す。
//
// 例：
//   skipelem("a/bb/c", name) = "bb/c", name = "a"をセット
//   skipelem("///a//bb", name) = "bb", name = "a"をセット
//   skipelem("a", name) = "", name = "a"をセット
//   skipelem("", name) = skipelem("////", name) = 0
//
static char*
skipelem(char *path, char *name)
{
  char *s;
  int len;

  while(*path == '/')
    path++;
  if(*path == 0)
    return 0;
  s = path;
  while(*path != '/' && *path != 0)
    path++;
  len = path - s;
  if(len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  while(*path == '/')
    path++;
  return path;
}

// パス名のinodeを探して返す関数である。
// parent != 0の場合、親のinodeを返し、最終パス要素をnameにコピーする。
// nameにはDIRSIZバイト分の余裕が必要である。
// iput()を呼び出すため、トランザクション内で呼び出す必要がある。
static struct inode*
namex(char *path, int nameiparent, char *name)
{
  struct inode *ip, *next;

  if(*path == '/')
    ip = iget(ROOTDEV, ROOTINO);
  else
    ip = idup(myproc()->cwd);

  while((path = skipelem(path, name)) != 0){
    ilock(ip);
    if(ip->type != T_DIR){
      iunlockput(ip);
      return 0;
    }
    if(nameiparent && *path == '\0'){
      // 一つのレベル早く停止する。
      iunlock(ip);
      return ip;
    }
    if((next = dirlookup(ip, name, 0)) == 0){
      iunlockput(ip);
      return 0;
    }
    iunlockput(ip);
    ip = next;
  }
  if(nameiparent){
    iput(ip);
    return 0;
  }
  return ip;
}

// パス名のinodeを返す関数である。
struct inode*
namei(char *path)
{
  char name[DIRSIZ];
  return namex(path, 0, name);
}

// 親のパス名のinodeを返し、最終パス要素をnameにコピーする関数である。
struct inode*
nameiparent(char *path, char *name)
{
  return namex(path, 1, name);
}
