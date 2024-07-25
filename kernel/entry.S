        # qemu -kernel はカーネルを0x80000000にロードし、
        # 各ハート（すなわちCPU）をそこにジャンプさせる。
        # kernel.ld は以下のコードを
        # 0x80000000に配置する。
.section .text
.global _entry
_entry:
        # C言語用のスタックを設定する。
        # stack0 は start.c に宣言されており、
        # 各CPUに4096バイトのスタックが割り当てられている。
        # sp = stack0 + (hartid * 4096)
        la sp, stack0       # スタックのベースアドレスをロードする。
        li a0, 1024*4       # 4096バイトをロードする。
        csrr a1, mhartid    # 現在のハートIDを取得する。
        addi a1, a1, 1      # ハートIDを1増加させる。
        mul a0, a0, a1      # 4096バイトにハートIDを掛け算する。
        add sp, sp, a0      # スタックポインタを計算する。
        # start.c の start() にジャンプする。
        call start
spin:
        j spin              # 無限ループでスピンする。
