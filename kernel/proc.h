// カーネルコンテキストスイッチ用に保存されるレジスタ
struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved（呼び出し側で保存されるレジスタ）
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// 各CPUの状態を管理する構造体
struct cpu {
  struct proc *proc;          // このCPU上で動作しているプロセス、またはnull
  struct context context;     // スケジューラに入るためのswtch()用のコンテキスト
  int noff;                   // push_off()のネスト深度=割り込みが無効化された回数
  int warikomi;               // push_off()前の割り込みの有効状態
};

extern struct cpu cpus[NCPU]; // 全CPUの状態を保持する配列

// トラップ処理用のプロセスごとのデータ。trampoline.S内のトラップ処理コード用。
// ユーザページテーブル内のトランポリンページのすぐ下のページに配置。
// カーネルページテーブルには特にマッピングされない。
// trampoline.S内のuservecはユーザレジスタをtrapframeに保存し、
// kernel_sp、kernel_hartid、kernel_satpからレジスタを初期化し、kernel_trapにジャンプする。
// usertrapret()およびtrampoline.S内のuserretはtrapframeのkernel_*を設定し、
// trapframeからユーザレジスタを復元し、ユーザページテーブルに切り替えてユーザ空間に入る。
// trapframeにはs0-s11のようなcallee-savedのユーザレジスタが含まれている。
// これはusertrapret()経由でユーザに戻る経路がカーネルコールスタック全体を経由しないため。
struct trapframe {
  /*   0 */ uint64 kernel_satp;   // カーネルページテーブル
  /*   8 */ uint64 kernel_sp;     // プロセスのカーネルスタックのトップ
  /*  16 */ uint64 kernel_trap;   // usertrap()へのポインタ
  /*  24 */ uint64 epc;           // 保存されたユーザプログラムカウンタ
  /*  32 */ uint64 kernel_hartid; // 保存されたカーネルtp
  /*  40 */ uint64 ra;
  /*  48 */ uint64 sp;
  /*  56 */ uint64 gp;
  /*  64 */ uint64 tp;
  /*  72 */ uint64 t0;
  /*  80 */ uint64 t1;
  /*  88 */ uint64 t2;
  /*  96 */ uint64 s0;
  /* 104 */ uint64 s1;
  /* 112 */ uint64 a0;
  /* 120 */ uint64 a1;
  /* 128 */ uint64 a2;
  /* 136 */ uint64 a3;
  /* 144 */ uint64 a4;
  /* 152 */ uint64 a5;
  /* 160 */ uint64 a6;
  /* 168 */ uint64 a7;
  /* 176 */ uint64 s2;
  /* 184 */ uint64 s3;
  /* 192 */ uint64 s4;
  /* 200 */ uint64 s5;
  /* 208 */ uint64 s6;
  /* 216 */ uint64 s7;
  /* 224 */ uint64 s8;
  /* 232 */ uint64 s9;
  /* 240 */ uint64 s10;
  /* 248 */ uint64 s11;
  /* 256 */ uint64 t3;
  /* 264 */ uint64 t4;
  /* 272 */ uint64 t5;
  /* 280 */ uint64 t6;
};

// プロセスの状態を表す列挙型
enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// 各プロセスの状態を管理する構造体
struct proc {
  struct spinlock lock;        // プロセス固有のロック

  // p->lockが保持されている間に使用されるべきフィールド：
  enum procstate state;        // プロセスの状態
  void *chan;                  // 非ゼロの場合、このチャネルでスリープ中
  int killed;                  // 非ゼロの場合、killされている
  int xstate;                  // 親のwaitのために返される終了ステータス
  int pid;                     // プロセスID

  // wait_lockが保持されている間に使用されるべきフィールド：
  struct proc *parent;         // 親プロセス

  // プロセスにプライベートなフィールドなので、p->lockを保持する必要はない：
  uint64 kstack;               // カーネルスタックの仮想アドレス
  uint64 sz;                   // プロセスメモリのサイズ（バイト単位）
  pagetable_t pagetable;       // ユーザページテーブル
  struct trapframe *trapframe; // trampoline.S用のデータページ
  struct context context;      // プロセスを実行するためのswtch()用のコンテキスト
  struct file *ofile[NOFILE];  // オープンファイル
  struct inode *cwd;           // カレントディレクトリ
  char name[16];               // プロセス名（デバッグ用）
};
