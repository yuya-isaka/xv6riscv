#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void main();
void timerinit();

// entry.S が各CPU用に必要とするスタック
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

// entry.S がmachineモードで stack0 上にジャンプする
void
start()
{
  // M 前の特権モードを Supervisor に設定するために mret を使用
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  // M 例外プログラムカウンタを main に設定するために mret を使用
  // gcc -mcmodel=medany を要求する
  w_mepc((uint64)main);

  // 現在のところ、ページングを無効にする
  w_satp(0);

  // すべての割り込みと例外を Supervisor モードに委譲する
  w_medeleg(0xffff);
  w_mideleg(0xffff);
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

  // 物理メモリ保護を設定し、Supervisor モードにすべての物理メモリへのアクセスを許可する
  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf);

  // クロック割り込みを要求する
  timerinit();

  // 各CPUの hartid を tp レジスタに保存し、 cpuid() のために使用
  int id = r_mhartid();
  w_tp(id);

  // Supervisor モードに切り替え、 main() にジャンプする
  asm volatile("mret");
}

// 各 hart にタイマー割り込みを生成するよう要求する
void
timerinit()
{
  // Supervisor モードのタイマー割り込みを有効にする
  w_mie(r_mie() | MIE_STIE);

  // sstc 拡張 (例えば stimecmp) を有効にする
  w_menvcfg(r_menvcfg() | (1L << 63));

  // Supervisor に stimecmp と time の使用を許可する
  w_mcounteren(r_mcounteren() | 2);

  // 最初のタイマー割り込みを要求する
  w_stimecmp(r_time() + 1000000);
}
