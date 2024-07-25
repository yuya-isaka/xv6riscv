OUTPUT_ARCH( "riscv" )
ENTRY( _entry )

SECTIONS
{
  /*
   * entry.S / _entry が0x80000000にあることを保証する。
   * これはqemuの-kernelオプションがジャンプする場所である。
   */
  . = 0x80000000;

  .text : {
    *(.text .text.*)       /* .textセクションとそのサブセクションをすべて含む。 */
    . = ALIGN(0x1000);     /* アドレスを0x1000の境界にアラインする。 */
    _trampoline = .;       /* _trampolineラベルを現在のアドレスに設定する。 */
    *(trampsec)            /* trampsecセクションを含む。 */
    . = ALIGN(0x1000);     /* アドレスを再び0x1000の境界にアラインする。 */
    ASSERT(. - _trampoline == 0x1000, "error: trampoline larger than one page"); /* トランポリンが1ページを超えないことを確認する。 */
    PROVIDE(etext = .);    /* etextシンボルを現在のアドレスに設定する。*/
  }

  .rodata : {
    . = ALIGN(16);         /* アドレスを16の境界にアラインする。 */
    *(.srodata .srodata.*) /* .srodataセクションとそのサブセクションを含む。 */
    . = ALIGN(16);         /* アドレスを再び16の境界にアラインする。 */
    *(.rodata .rodata.*)   /* .rodataセクションとそのサブセクションを含む。 */
  }

  .data : {
    . = ALIGN(16);         /* アドレスを16の境界にアラインする。 */
    *(.sdata .sdata.*)     /* .sdataセクションとそのサブセクションを含む。 */
    . = ALIGN(16);         /* アドレスを再び16の境界にアラインする。 */
    *(.data .data.*)       /* .dataセクションとそのサブセクションを含む。 */
  }

  .bss : {
    . = ALIGN(16);         /* アドレスを16の境界にアラインする。 */
    *(.sbss .sbss.*)       /* .sbssセクションとそのサブセクションを含む。 */
    . = ALIGN(16);         /* アドレスを再び16の境界にアラインする。 */
    *(.bss .bss.*)         /* .bssセクションとそのサブセクションを含む。 */
  }

  PROVIDE(end = .);        /* endシンボルを現在のアドレスに設定する。 */
}
