// プロセス用の長期ロック
struct sleeplock {
  uint locked;        // ロックが保持されているかどうか
  struct spinlock lk; // このスリープロックを保護するスピンロック

  // デバッグ用:
  char *name;         // ロックの名前
  int pid;            // ロックを保持しているプロセスID
};
