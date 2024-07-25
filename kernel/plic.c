#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

//
// RISC-V プラットフォームレベル割り込みコントローラ (PLIC).
//

// PLICの初期化
void
plicinit(void)
{
  // 必要なIRQの優先度を非ゼロに設定する（ゼロの場合は無効）。
  *(uint32*)(PLIC + UART0_IRQ*4) = 1;
  *(uint32*)(PLIC + VIRTIO0_IRQ*4) = 1;
}

// 各ハート（CPUコア）用のPLICの初期化
void
plicinithart(void)
{
  int hart = cpuid();

  // このハートのSモード用の有効ビットを設定する
  // UARTおよびVirtioディスクのため。
  *(uint32*)PLIC_SENABLE(hart) = (1 << UART0_IRQ) | (1 << VIRTIO0_IRQ);

  // このハートのSモードの優先度閾値を0に設定する。
  *(uint32*)PLIC_SPRIORITY(hart) = 0;
}

// PLICにどの割り込みを処理するべきか尋ねる。
int
plic_claim(void)
{
  int hart = cpuid();
  int irq = *(uint32*)PLIC_SCLAIM(hart);
  return irq;
}

// PLICにこのIRQを処理したことを伝える。
void
plic_complete(int irq)
{
  int hart = cpuid();
  *(uint32*)PLIC_SCLAIM(hart) = irq;
}
