struct buf;
struct context;
struct file;
struct inode;
struct pipe;
struct proc;
struct spinlock;
struct sleeplock;
struct stat;
struct superblock;

// bio.c
void            binit(void);                            // バッファキャッシュを初期化する関数である。
struct buf*     bread(uint, uint);                      // ディスクからバッファにブロックを読み込む関数である。
void            brelse(struct buf*);                    // バッファを解放する関数である。
void            bwrite(struct buf*);                    // バッファの内容をディスクに書き込む関数である。
void            bpin(struct buf*);                      // バッファを固定する関数である。
void            bunpin(struct buf*);                    // バッファの固定を解除する関数である。

// console.c
void            consoleinit(void);                      // コンソールを初期化する関数である。
void            consoleintr(int);                       // コンソールの割り込み処理関数である。
void            consputc(int);                          // コンソールに文字を出力する関数である。

// exec.c
int             exec(char*, char**);                    // 新しいプログラムを実行する関数である。

// file.c
struct file*    filealloc(void);                        // ファイル構造体を割り当てる関数である。
void            fileclose(struct file*);                // ファイルを閉じる関数である。
struct file*    filedup(struct file*);                  // ファイル構造体の参照カウントを増加させる関数である。
void            fileinit(void);                         // ファイルシステムを初期化する関数である。
int             fileread(struct file*, uint64, int n);  // ファイルからデータを読み取る関数である。
int             filestat(struct file*, uint64 addr);    // ファイルのメタデータを取得する関数である。
int             filewrite(struct file*, uint64, int n); // ファイルにデータを書き込む関数である。

// fs.c
void            fsinit(int);                            // ファイルシステムを初期化する関数である。
int             dirlink(struct inode*, char*, uint);    // ディレクトリエントリをリンクする関数である。
struct inode*   dirlookup(struct inode*, char*, uint*); // ディレクトリエントリを検索する関数である。
struct inode*   ialloc(uint, short);                    // inodeを割り当てる関数である。
struct inode*   idup(struct inode*);                    // inodeの参照カウントを増加させる関数である。
void            iinit(void);                            // inodeシステムを初期化する関数である。
void            ilock(struct inode*);                   // inodeをロックする関数である。
void            iput(struct inode*);                    // inodeを解放する関数である。
void            iunlock(struct inode*);                 // inodeのロックを解除する関数である。
void            iunlockput(struct inode*);              // inodeのロックを解除し、解放する関数である。
void            iupdate(struct inode*);                 // inodeをディスクに更新する関数である。
int             namecmp(const char*, const char*);      // 2つの名前を比較する関数である。
struct inode*   namei(char*);                           // パス名のinodeを取得する関数である。
struct inode*   nameiparent(char*, char*);              // 親のパス名のinodeを取得する関数である。
int             readi(struct inode*, int, uint64, uint, uint); // inodeからデータを読み取る関数である。
void            stati(struct inode*, struct stat*);     // inodeのstat情報を取得する関数である。
int             writei(struct inode*, int, uint64, uint, uint); // inodeにデータを書き込む関数である。
void            itrunc(struct inode*);                  // inodeをトランケートする関数である。

// ramdisk.c
void            ramdiskinit(void);                      // RAMディスクを初期化する関数である。
void            ramdiskintr(void);                      // RAMディスクの割り込み処理関数である。
void            ramdiskrw(struct buf*);                 // RAMディスクの読み書き関数である。
int             either_copyin(void *dst, int user_src, uint64 src, uint64 len);  // カーネルまたはユーザー空間からデータをコピーする関数である。
void            procdump(void);                        // プロセス情報をダンプする関数である。

// swtch.S
void            swtch(struct context*, struct context*); // コンテキストスイッチを行う関数である。

// spinlock.c
void            acquire(struct spinlock*);             // スピンロックを取得する関数である。
int             holding(struct spinlock*);             // スピンロックを保持しているか確認する関数である。
void            initlock(struct spinlock*, char*);     // スピンロックを初期化する関数である。
void            release(struct spinlock*);             // スピンロックを解放する関数である。
void            push_off(void);                        // 割り込みを無効にする関数である。
void            pop_off(void);                         // 割り込みを有効にする関数である。

// sleeplock.c
void            acquiresleep(struct sleeplock*);       // スリープロックを取得する関数である。
void            releasesleep(struct sleeplock*);       // スリープロックを解放する関数である。
int             holdingsleep(struct sleeplock*);       // スリープロックを保持しているか確認する関数である。
void            initsleeplock(struct sleeplock*, char*); // スリープロックを初期化する関数である。

// string.c
int             memcmp(const void*, const void*, uint); // メモリを比較する関数である。
void*           memmove(void*, const void*, uint);      // メモリを移動する関数である。
void*           memset(void*, int, uint);               // メモリを特定の値で埋める関数である。
char*           safestrcpy(char*, const char*, int);    // 安全に文字列をコピーする関数である。
int             strlen(const char*);                    // 文字列の長さを計測する関数である。
int             strncmp(const char*, const char*, uint);// 文字列を比較する関数である。
char*           strncpy(char*, const char*, int);       // 文字列を指定された長さだけコピーする関数である。

// syscall.c
void            argint(int, int*);                      // システムコール引数を整数として取得する関数である。
int             argstr(int, char*, int);                // システムコール引数を文字列として取得する関数である。
void            argaddr(int, uint64*);                  // システムコール引数をアドレスとして取得する関数である。
int             fetchstr(uint64, char*, int);           // ユーザー空間からnull終端文字列を取得する関数である。
int             fetchaddr(uint64, uint64*);             // ユーザー空間からアドレスを取得する関数である。
void            syscall(void);                          // システムコールを処理する関数である。

// trap.c
extern uint     ticks;                                  // システムのティックカウントである。
void            trapinit(void);                         // トラップを初期化する関数である。
void            trapinithart(void);                     // ハートトラップを初期化する関数である。
extern struct spinlock tickslock;                       // ティックカウント用のスピンロックである。
void            usertrapret(void);                      // ユーザートラップからの復帰を処理する関数である。

// uart.c
void            uartinit(void);                         // UARTを初期化する関数である。
void            uartintr(void);                         // UARTの割り込み処理関数である。
void            uartputc(int);                          // UARTに文字を出力する関数である。
void            uartputc_sync(int);                     // UARTに文字を同期的に出力する関数である。
int             uartgetc(void);                         // UARTから文字を入力する関数である。

// vm.c
void            kvminit(void);                          // カーネル仮想メモリを初期化する関数である。
void            kvminithart(void);                      // ハート仮想メモリを初期化する関数である。
void            kvmmap(pagetable_t, uint64, uint64, uint64, int); // カーネル仮想メモリをマッピングする関数である。
int             mappages(pagetable_t, uint64, uint64, uint64, int); // ページをマッピングする関数である。
pagetable_t     uvmcreate(void);                        // ユーザー仮想メモリを作成する関数である。
void            uvmfirst(pagetable_t, uchar *, uint);   // 最初のページをユーザー仮想メモリにロードする関数である。
uint64          uvmalloc(pagetable_t, uint64, uint64, int); // ユーザー仮想メモリにページを割り当てる関数である。
uint64          uvmdealloc(pagetable_t, uint64, uint64); // ユーザー仮想メモリからページを解放する関数である。
int             uvmcopy(pagetable_t, pagetable_t, uint64); // ユーザー仮想メモリをコピーする関数である。
void            uvmfree(pagetable_t, uint64);           // ユーザー仮想メモリを解放する関数である。
void            uvmunmap(pagetable_t, uint64, uint64, int); // ユーザー仮想メモリのマッピングを解除する関数である。
void            uvmclear(pagetable_t, uint64);          // ユーザー仮想メモリをクリアする関数である。
pte_t*          walk(pagetable_t, uint64, int);         // ページテーブルをウォークする関数である。
uint64          walkaddr(pagetable_t, uint64);          // アドレスに対応する物理アドレスを取得する関数である。
int             copyout(pagetable_t, uint64, char*, uint64); // ユーザー空間にデータをコピーする関数である。
int             copyin(pagetable_t, char*, uint64, uint64);  // ユーザー空間からデータをコピーする関数である。
int             copyinstr(pagetable_t, char*, uint64, uint64); // ユーザー空間から文字列をコピーする関数である。

// plic.c
void            plicinit(void);                         // PLICを初期化する関数である。
void            plicinithart(void);                     // ハートPLICを初期化する関数である。
int             plic_claim(void);                       // PLICから割り込み要求を取得する関数である。
void            plic_complete(int);                     // PLICへの割り込み処理完了を通知する関数である。

// virtio_disk.c
void            virtio_disk_init(void);                 // Virtioディスクを初期化する関数である。
void            virtio_disk_rw(struct buf *, int);      // Virtioディスクの読み書きを行う関数である。
void            virtio_disk_intr(void);                 // Virtioディスクの割り込み処理関数である。

// 固定サイズ配列の要素数である。
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

// kalloc.c
void*           kalloc(void);                           // カーネルメモリを割り当てる関数である。
void            kfree(void*);                           // カーネルメモリを解放する関数である。
void            kinit(void);                            // カーネルメモリの初期化関数である。

// log.c
void            initlog(int, struct superblock*);       // ログを初期化する関数である。
void            log_write(struct buf*);                 // バッファの内容をログに書き込む関数である。
void            begin_op(void);                         // ログ操作の開始を通知する関数である。
void            end_op(void);                           // ログ操作の終了を通知する関数である。

// pipe.c
int             pipealloc(struct file**, struct file**);// パイプを割り当てる関数である。
void            pipeclose(struct pipe*, int);           // パイプを閉じる関数である。
int             piperead(struct pipe*, uint64, int);    // パイプからデータを読み取る関数である。
int             pipewrite(struct pipe*, uint64, int);   // パイプにデータを書き込む関数である。

// printf.c
int             printf(char*, ...) __attribute__ ((format (printf, 1, 2))); // フォーマットに従ってコンソールに出力する関数である。
void            panic(char*) __attribute__((noreturn));                     // パニックメッセージを表示し、システムを停止する関数である。
void            printfinit(void);                                           // printfシステムを初期化する関数である。

// proc.c
int             cpuid(void);                          // 現在のCPU IDを取得する関数である。
void            exit(int);                            // プロセスを終了する関数である。
int             fork(void);                           // 新しいプロセスを生成する関数である。
int             growproc(int);                        // プロセスのメモリサイズを変更する関数である。
void            proc_mapstacks(pagetable_t);          // スタックをマッピングする関数である。
pagetable_t     proc_pagetable(struct proc*);         // プロセスのページテーブルを取得する関数である。
void            proc_freepagetable(pagetable_t, uint64); // プロセスのページテーブルを解放する関数である。
int             kill(int);                            // プロセスを終了させる関数である。
int             killed(struct proc*);                 // プロセスが終了予定かを確認する関数である。
void            setkilled(struct proc*);              // プロセスを終了予定に設定する関数である。
struct cpu*     mycpu(void);                          // 現在のCPU構造体を取得する関数である。
struct cpu*     getmycpu(void);                       // 現在のCPU構造体を取得する関数である。
struct proc*    myproc(void);                         // 現在のプロセス構造体を取得する関数である。
void            procinit(void);                       // プロセスシステムを初期化する関数である。
void            scheduler(void) __attribute__((noreturn)); // スケジューラを起動する関数である。
void            sched(void);                          // プロセスのスケジューリングを行う関数である。
void            sleep(void*, struct spinlock*);       // プロセスをスリープさせる関数である。
void            userinit(void);                       // ユーザープロセスを初期化する関数である。
int             wait(uint64);                         // プロセスの終了を待機する関数である。
void            wakeup(void*);                        // スリープ中のプロセスを起床させる関数である。
void            yield(void);                          // プロセスの実行を一時停止し、他のプロセスに切り替える関数である。
int             either_copyout(int user_dst, uint64 dst, void *src, uint64 len); // カーネルまたはユーザー空間にデータをコピーする関数である。
