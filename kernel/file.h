struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type; // ファイルの種類を示す列挙型である。
  int ref; // 参照カウントである。
  char readable; // 読み取り可能かどうかを示すフラグである。
  char writable; // 書き込み可能かどうかを示すフラグである。
  struct pipe *pipe; // typeがFD_PIPEの場合に使用されるパイプ構造体へのポインタである。
  struct inode *ip;  // typeがFD_INODEおよびFD_DEVICEの場合に使用されるinode構造体へのポインタである。
  uint off;          // typeがFD_INODEの場合のファイルオフセットである。
  short major;       // typeがFD_DEVICEの場合のメジャーデバイス番号である。
};

#define major(dev)  ((dev) >> 16 & 0xFFFF) // デバイス番号からメジャーデバイス番号を取得するマクロである。
#define minor(dev)  ((dev) & 0xFFFF)       // デバイス番号からマイナーデバイス番号を取得するマクロである。
#define mkdev(m,n)  ((uint)((m)<<16| (n))) // メジャーおよびマイナーデバイス番号からデバイス番号を作成するマクロである。

// メモリ上のinodeのコピーである。
struct inode {
  uint dev;           // デバイス番号である。
  uint inum;          // inode番号である。
  int ref;            // 参照カウントである。
  struct sleeplock lock; // 以下のすべてのフィールドを保護するスリープロックである。
  int valid;          // inodeがディスクから読み込まれているかどうかを示すフラグである。

  short type;         // ディスクinodeのコピーである。
  short major;        // メジャーデバイス番号である。
  short minor;        // マイナーデバイス番号である。
  short nlink;        // ハードリンクの数である。
  uint size;          // ファイルサイズである。
  uint addrs[NDIRECT+1]; // ディスクブロックアドレスの配列である。
};

// メジャーデバイス番号をデバイス関数にマップする構造体である。
struct devsw {
  int (*read)(int, uint64, int);  // デバイスの読み取り関数へのポインタである。
  int (*write)(int, uint64, int); // デバイスの書き込み関数へのポインタである。
};

extern struct devsw devsw[]; // devsw配列の外部宣言である。

#define CONSOLE 1 // コンソールデバイスのメジャーデバイス番号である。
