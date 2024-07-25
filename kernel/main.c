#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

volatile static int started = 0;

// start() はすべてのCPUでスーパーバイザーモードでここにジャンプする。
void
main()
{
  if(cpuid() == 0){
    // 0番目のCPUのみが以下の初期化を行う。
    consoleinit();    // コンソールの初期化
    printfinit();     // printfの初期化
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    kinit();          // 物理ページアロケータ
    kvminit();        // カーネルページテーブルの作成
    kvminithart();    // ページングを有効にする
    procinit();       // プロセステーブルの初期化
    trapinit();       // トラップベクターの初期化
    trapinithart();   // カーネルトラップベクターのインストール
    plicinit();       // 割り込みコントローラのセットアップ
    plicinithart();   // デバイス割り込みのためにPLICにリクエスト
    binit();          // バッファキャッシュの初期化
    iinit();          // inodeテーブルの初期化
    fileinit();       // ファイルテーブルの初期化
    virtio_disk_init(); // エミュレートされたハードディスクの初期化
    userinit();       // 最初のユーザープロセスの初期化
    __sync_synchronize();
    started = 1;
  } else {
    // 他のCPUは、初期化が完了するまで待機する。
    while(started == 0)
      ;
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();    // ページングを有効にする
    trapinithart();   // カーネルトラップベクターのインストール
    plicinithart();   // デバイス割り込みのためにPLICにリクエスト
  }

  scheduler();        // スケジューラを開始
}
