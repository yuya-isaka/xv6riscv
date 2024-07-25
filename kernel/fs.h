// ディスク上のファイルシステムフォーマット。
// カーネルとユーザープログラムの両方がこのヘッダーファイルを使用する。

#define ROOTINO  1   // ルートi-number
#define BSIZE 1024  // ブロックサイズ

// ディスクレイアウト:
// [ ブートブロック | スーパーブロック | ログ | inodeブロック |
//                                          フリービットマップ | データブロック]
//
// mkfsはスーパーブロックを計算し、初期ファイルシステムを構築する。
// スーパーブロックはディスクレイアウトを記述する。
struct superblock {
  uint magic;        // FSMAGICである必要がある。
  uint size;         // ファイルシステムイメージのサイズ（ブロック数）。
  uint nblocks;      // データブロックの数。
  uint ninodes;      // inodeの数。
  uint nlog;         // ログブロックの数。
  uint logstart;     // 最初のログブロックのブロック番号。
  uint inodestart;   // 最初のinodeブロックのブロック番号。
  uint bmapstart;    // 最初のフリーマップブロックのブロック番号。
};

#define FSMAGIC 0x10203040

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

// ディスク上のinode構造体
struct dinode {
  short type;           // ファイルタイプ。
  short major;          // メジャーデバイス番号（T_DEVICEのみ）。
  short minor;          // マイナーデバイス番号（T_DEVICEのみ）。
  short nlink;          // ファイルシステム内のinodeへのリンク数。
  uint size;            // ファイルサイズ（バイト単位）。
  uint addrs[NDIRECT+1];   // データブロックのアドレス。
};

// ブロックあたりのinode数。
#define IPB           (BSIZE / sizeof(struct dinode))

// inode iを含むブロック
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// ブロックあたりのビットマップビット数
#define BPB           (BSIZE*8)

// ブロックbのビットを含むフリーマップのブロック
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)

// ディレクトリはdirent構造体のシーケンスを含むファイルである。
#define DIRSIZ 14

struct dirent {
  ushort inum;          // inode番号。
  char name[DIRSIZ];    // 名前。
};
