// ミューテックスロック
struct spinlock {
  uint locked;       // ロックが保持されているか？

  // デバッグ用
  char *name;        // ロックの名前
  struct cpu *cpu;   // ロックを保持しているCPU
};
