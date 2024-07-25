#define T_DIR     1   // ディレクトリである
#define T_FILE    2   // 通常のファイルである
#define T_DEVICE  3   // デバイスファイルである

// ファイルシステムの統計情報を表す構造体である
struct stat {
  int dev;     // ファイルシステムのディスクデバイスである
  uint ino;    // inode番号である
  short type;  // ファイルの種類である
  short nlink; // ファイルへのリンク数である
  uint64 size; // ファイルのサイズ（バイト単位）である
};
