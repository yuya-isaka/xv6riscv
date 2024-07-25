// 物理メモリレイアウト

// qemu -machine virtは次のように設定されている。
// これはqemuのhw/riscv/virt.cに基づいている:
//
// 00001000 -- ブートROM、qemuによって提供される
// 02000000 -- CLINT
// 0C000000 -- PLIC
// 10000000 -- uart0
// 10001000 -- virtioディスク
// 80000000 -- ブートROMがマシンモードでここにジャンプする
//             -kernelはここにカーネルをロードする
// 80000000以降の未使用RAM。

// カーネルは物理メモリを次のように使用する:
// 80000000 -- entry.S、その後カーネルテキストとデータ
// end -- カーネルページ割り当て領域の開始
// PHYSTOP -- カーネルが使用するRAMの終わり

// qemuはUARTレジスタを物理メモリのここに配置する。
#define UART0 0x10000000L
#define UART0_IRQ 10

// virtio mmioインターフェース
#define VIRTIO0 0x10001000
#define VIRTIO0_IRQ 1

// qemuはプラットフォームレベル割り込みコントローラ（PLIC）をここに配置する。
#define PLIC 0x0c000000L
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)

// カーネルはRAMが存在すると予期している。
// カーネルとユーザーページの使用のために、物理アドレス0x80000000からPHYSTOPまで。
#define KERNBASE 0x80000000L
#define PHYSTOP (KERNBASE + 128*1024*1024)

// トランポリンページを最も高いアドレスにマップする。
// ユーザー空間とカーネル空間の両方で。
#define TRAMPOLINE (MAXVA - PGSIZE)

// カーネルスタックをトランポリンの下にマップする。
// それぞれが無効なガードページで囲まれている。
#define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*PGSIZE)

// ユーザーメモリレイアウト。
// アドレス0から開始:
//   テキスト
//   元のデータとbss
//   固定サイズのスタック
//   拡張可能なヒープ
//   ...
//   TRAPFRAME (p->trapframe、トランポリンによって使用される)
//   TRAMPOLINE (カーネルと同じページ)
#define TRAPFRAME (TRAMPOLINE - PGSIZE)
