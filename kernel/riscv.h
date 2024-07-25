#ifndef __ASSEMBLER__

// 現在のハート（コア）を取得
static inline uint64
r_mhartid()
{
  uint64 x;
  asm volatile("csrr %0, mhartid" : "=r" (x) );
  return x;
}

// Machine Status Register (mstatus) の定義
#define MSTATUS_MPP_MASK (3L << 11) // previous modeのマスク
#define MSTATUS_MPP_M (3L << 11) // Machine mode
#define MSTATUS_MPP_S (1L << 11) // Supervisor mode
#define MSTATUS_MPP_U (0L << 11) // User mode
#define MSTATUS_MIE (1L << 3)    // machine-mode interrupt enable

// mstatusレジスタの読み込み
static inline uint64
r_mstatus()
{
  uint64 x;
  asm volatile("csrr %0, mstatus" : "=r" (x) );
  return x;
}

// mstatusレジスタの書き込み
static inline void
w_mstatus(uint64 x)
{
  asm volatile("csrw mstatus, %0" : : "r" (x));
}

// Machine Exception Program Counter (mepc) の書き込み
static inline void
w_mepc(uint64 x)
{
  asm volatile("csrw mepc, %0" : : "r" (x));
}

// Supervisor Status Register (sstatus) の定義
#define SSTATUS_SPP (1L << 8)  // Previous mode, 1=Supervisor, 0=User
#define SSTATUS_SPIE (1L << 5) // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4) // User Previous Interrupt Enable
#define SSTATUS_SIE (1L << 1)  // Supervisor Interrupt Enable
#define SSTATUS_UIE (1L << 0)  // User Interrupt Enable

// sstatusレジスタの読み込み
static inline uint64
r_sstatus()
{
  uint64 x;
  asm volatile("csrr %0, sstatus" : "=r" (x) );
  return x;
}

// sstatusレジスタの書き込み
static inline void
w_sstatus(uint64 x)
{
  asm volatile("csrw sstatus, %0" : : "r" (x));
}

// Supervisor Interrupt Pendingの読み込み
static inline uint64
r_sip()
{
  uint64 x;
  asm volatile("csrr %0, sip" : "=r" (x) );
  return x;
}

// Supervisor Interrupt Pendingの書き込み
static inline void
w_sip(uint64 x)
{
  asm volatile("csrw sip, %0" : : "r" (x));
}

// Supervisor Interrupt Enableの定義
#define SIE_SEIE (1L << 9) // external
#define SIE_STIE (1L << 5) // timer
#define SIE_SSIE (1L << 1) // software

// sieレジスタの読み込み
static inline uint64
r_sie()
{
  uint64 x;
  asm volatile("csrr %0, sie" : "=r" (x) );
  return x;
}

// sieレジスタの書き込み
static inline void
w_sie(uint64 x)
{
  asm volatile("csrw sie, %0" : : "r" (x));
}

// Machine-mode Interrupt Enableの定義
#define MIE_STIE (1L << 5)  // supervisor timer

// mieレジスタの読み込み
static inline uint64
r_mie()
{
  uint64 x;
  asm volatile("csrr %0, mie" : "=r" (x) );
  return x;
}

// mieレジスタの書き込み
static inline void
w_mie(uint64 x)
{
  asm volatile("csrw mie, %0" : : "r" (x));
}

// Supervisor Exception Program Counter (sepc) の書き込み
static inline void
w_sepc(uint64 x)
{
  asm volatile("csrw sepc, %0" : : "r" (x));
}

// sepcレジスタの読み込み
static inline uint64
r_sepc()
{
  uint64 x;
  asm volatile("csrr %0, sepc" : "=r" (x) );
  return x;
}

// Machine Exception Delegationの読み込み
static inline uint64
r_medeleg()
{
  uint64 x;
  asm volatile("csrr %0, medeleg" : "=r" (x) );
  return x;
}

// Machine Exception Delegationの書き込み
static inline void
w_medeleg(uint64 x)
{
  asm volatile("csrw medeleg, %0" : : "r" (x));
}

// Machine Interrupt Delegationの読み込み
static inline uint64
r_mideleg()
{
  uint64 x;
  asm volatile("csrr %0, mideleg" : "=r" (x) );
  return x;
}

// Machine Interrupt Delegationの書き込み
static inline void
w_mideleg(uint64 x)
{
  asm volatile("csrw mideleg, %0" : : "r" (x));
}

// Supervisor Trap-Vector Base Addressの書き込み
static inline void
w_stvec(uint64 x)
{
  asm volatile("csrw stvec, %0" : : "r" (x));
}

// stvecレジスタの読み込み
static inline uint64
r_stvec()
{
  uint64 x;
  asm volatile("csrr %0, stvec" : "=r" (x) );
  return x;
}

// Supervisor Timer Comparison Registerの読み込み
static inline uint64
r_stimecmp()
{
  uint64 x;
  asm volatile("csrr %0, stimecmp" : "=r" (x) );
  return x;
}

// stimecmpレジスタの書き込み
static inline void
w_stimecmp(uint64 x)
{
  asm volatile("csrw stimecmp, %0" : : "r" (x));
}

// Machine Environment Configuration Registerの読み込み
static inline uint64
r_menvcfg()
{
  uint64 x;
  asm volatile("csrr %0, menvcfg" : "=r" (x) );
  return x;
}

// menvcfgレジスタの書き込み
static inline void
w_menvcfg(uint64 x)
{
  asm volatile("csrw menvcfg, %0" : : "r" (x));
}

// Physical Memory Protection (PMP) レジスタの書き込み
static inline void
w_pmpcfg0(uint64 x)
{
  asm volatile("csrw pmpcfg0, %0" : : "r" (x));
}

static inline void
w_pmpaddr0(uint64 x)
{
  asm volatile("csrw pmpaddr0, %0" : : "r" (x));
}

// RISC-Vのsv39ページテーブル方式を使用
#define SATP_SV39 (8L << 60)
#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))

// supervisor address translation and protection (satp) レジスタの書き込み
static inline void
w_satp(uint64 x)
{
  asm volatile("csrw satp, %0" : : "r" (x));
}

// satpレジスタの読み込み
static inline uint64
r_satp()
{
  uint64 x;
  asm volatile("csrr %0, satp" : "=r" (x) );
  return x;
}

// Supervisor Trap Causeの読み込み
static inline uint64
r_scause()
{
  uint64 x;
  asm volatile("csrr %0, scause" : "=r" (x) );
  return x;
}

// Supervisor Trap Valueの読み込み
static inline uint64
r_stval()
{
  uint64 x;
  asm volatile("csrr %0, stval" : "=r" (x) );
  return x;
}

// Machine-mode Counter-Enableレジスタの書き込み
static inline void
w_mcounteren(uint64 x)
{
  asm volatile("csrw mcounteren, %0" : : "r" (x));
}

// mcounterenレジスタの読み込み
static inline uint64
r_mcounteren()
{
  uint64 x;
  asm volatile("csrr %0, mcounteren" : "=r" (x) );
  return x;
}

// machine-mode cycle counterの読み込み
static inline uint64
r_time()
{
  uint64 x;
  asm volatile("csrr %0, time" : "=r" (x) );
  return x;
}

// デバイス割り込みの有効化
static inline void
intr_on()
{
  w_sstatus(r_sstatus() | SSTATUS_SIE);
}

// デバイス割り込みの無効化
static inline void
intr_off()
{
  w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

// デバイス割り込みが有効かどうか
static inline int
intr_get()
{
  uint64 x = r_sstatus();
  return (x & SSTATUS_SIE) != 0;
}

// スタックポインタの読み込み
static inline uint64
r_sp()
{
  uint64 x;
  asm volatile("mv %0, sp" : "=r" (x) );
  return x;
}

// tp（スレッドポインタ）の読み込み
static inline uint64
r_tp()
{
  uint64 x;
  asm volatile("mv %0, tp" : "=r" (x) );
  return x;
}

// tp（スレッドポインタ）の書き込み
static inline void
w_tp(uint64 x)
{
  asm volatile("mv tp, %0" : : "r" (x));
}

// ra（リターンアドレス）の読み込み
static inline uint64
r_ra()
{
  uint64 x;
  asm volatile("mv %0, ra" : "=r" (x) );
  return x;
}

// TLBのフラッシュ
static inline void
sfence_vma()
{
  // ゼロ、ゼロはすべてのTLBエントリをフラッシュします。
  asm volatile("sfence.vma zero, zero");
}

typedef uint64 pte_t;
typedef uint64 *pagetable_t; // 512 PTEs

#endif // __ASSEMBLER__

#define PGSIZE 4096 // 1ページあたりのバイト数
#define PGSHIFT 12  // 1ページ内のオフセットビット数

#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))

#define PTE_V (1L << 0) // 有効ビット
#define PTE_R (1L << 1) // 読み取り可能ビット
#define PTE_W (1L << 2) // 書き込み可能ビット
#define PTE_X (1L << 3) // 実行可能ビット
#define PTE_U (1L << 4) // ユーザーアクセスビット

// 物理アドレスをPTEにシフト
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)

// PTEから物理アドレスを抽出
#define PTE2PA(pte) (((pte) >> 10) << 12)

// PTEフラグを抽出
#define PTE_FLAGS(pte) ((pte) & 0x3FF)

// 仮想アドレスから3つの9ビットのページテーブルインデックスを抽出
#define PXMASK          0x1FF // 9ビット
#define PXSHIFT(level)  (PGSHIFT+(9*(level)))
#define PX(level, va) ((((uint64) (va)) >> PXSHIFT(level)) & PXMASK)

// 最大仮想アドレスの1ビット超過。
// MAXVAは、Sv39によって許可される最大値よりも1ビット少なくなっています。
// これは、符号拡張を避けるためです。
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))
