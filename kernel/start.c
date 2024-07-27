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
  // mstatusレジスタ（Machineモード内のステータスレレジスタのMPP（前の特権命令）をSupervisor に設定
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  // 例外ハンドラ（start()）からのリターンアドレスを main に設定
  // gcc -mcmodel=medany を要求する
  w_mepc((uint64)main);

  // 現在のところ、ページングを無効にする
  w_satp(0);

  // Machineモードのすべての割り込みと例外を Supervisor モードに委譲する
  // すべての例外と割り込み（最大16種類）がSupervisorモードに委譲
  w_medeleg(0xffff);
  w_mideleg(0xffff);
  // 外部割り込み、タイマー割り込み、およびソフトウェア割り込みがSupervisorモードで有効になる
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

  // PMP
  // 参考文献: https://riscv.org/wp-content/uploads/2019/06/riscv-spec.pdf, https://msyksphinz.hatenablog.com/entry/2018/04/10/040000
  // S-/U-Modeの場合は, すべてのメモリアクセス(Read/Write/命令フェッチ)に対してPMPが適用
  // mstatusレジスタのMPRVビットが設定されており、mstatusレジスタのMPPフィールドにS-Mode/U-Modeが設定されている場合に、ロード・ストア操作に対してPMPが適用
  // (今回はMPRVビットは設定されていない)
  // PMPレジスタ自体がロックされている (pmpcfgのLockビットが設定されている）場合にはM-Modeでも適用される

  // PMPを使ってソフトウェアで物理メモリ保護を設定し、Supervisor モードにすべての物理メモリへのアクセスを許可する

  // 1. PMPで保護する物理メモリのアドレスを設定
  // 0x3fffffffffffffullは、64ビットのうち下位54ビットが全て1であることを意味する
  // （2進数で0011 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111）
  w_pmpaddr0(0x3fffffffffffffull);

  // 2.
  // 0xfは、4ビット全てが1であることを意味する（2進数で1111）
  // X/W/R: ビットを設定する
  // Aビットを1に設定：Top-of-rangeモード (TOR) : Top領域
  // TORモードは、PMPのアドレスマッチングモードの一つであり、保護するメモリの範囲を上位アドレス（Top）で指定する方法
  // 具体的には、2つの連続したPMPアドレスレジスタ (pmpaddr[i-1] と pmpaddr[i]) を使用して、保護するメモリ範囲を定義
  // メモリアクセスが行われる際、アクセスするアドレスが pmpaddr[i-1] と pmpaddr[i] の間にあるかどうかをチェック
  // アクセスするアドレスがこの範囲内にある場合、対応する pmpcfg[i] の R/W/X ビットの設定に基づいてアクセスが許可されるかどうかが決定

  // 今回設定しているのは0
  // pmpcfg0 の最初のエントリ（i = 0）の A ビットを TOR モード（01）に設定した場合、pmpaddr[0] が保護範囲の上限（上位アドレス）を示す
	// 範囲の下限は自動的にアドレス 0 になる
  // つまり、i = 0 の場合の TOR モードでは、アドレス範囲 0 から pmpaddr[0] までが保護対象となる
  w_pmpcfg0(0xf);

  // クロック割り込みを要求する
  timerinit();

  // 各CPUの hartid を tp レジスタに保存し、 cpuid() のために使用
  int id = r_mhartid();
  w_tp(id);

  // Supervisor モードに切り替え、 main() にジャンプする
  // mepcレジスタに格納されているアドレスにジャンプする (mepcレジスタには main() のアドレスが格納されている)
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
