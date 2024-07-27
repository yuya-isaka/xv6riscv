//
// 16550a UARTの低レベルドライバールーチン。
//

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

// UARTコントロールレジスタはUART0のアドレスにメモリマップされています。
// このマクロは、特定のレジスタのアドレスを返します。
#define Reg(reg) ((volatile unsigned char *)(UART0 + reg))

// UARTコントロールレジスタ。
// 一部のレジスタは読み取りと書き込みで意味が異なります。
// 詳細は http://byterunner.com/16550.html を参照してください。
#define RHR 0                 // 受信保持レジスタ（入力バイト用）
#define THR 0                 // 送信保持レジスタ（出力バイト用）
#define IER 1                 // 割り込み有効レジスタ
#define IER_RX_ENABLE (1<<0)
#define IER_TX_ENABLE (1<<1)
#define FCR 2                 // FIFOコントロールレジスタ
#define FCR_FIFO_ENABLE (1<<0)
#define FCR_FIFO_CLEAR (3<<1) // 2つのFIFOの内容をクリア
#define ISR 2                 // 割り込み状態レジスタ
#define LCR 3                 // ラインコントロールレジスタ
#define LCR_EIGHT_BITS (3<<0)
#define LCR_BAUD_LATCH (1<<7) // ボーレート設定の特別モード
#define LSR 5                 // ライン状態レジスタ
#define LSR_RX_READY (1<<0)   // 入力データがRHRに準備完了
#define LSR_TX_IDLE (1<<5)    // THRが次の送信バイトを受け入れる準備ができている

#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

// 送信用バッファ
struct spinlock uart_tx_lock;
#define UART_TX_BUF_SIZE 32
char uart_tx_buf[UART_TX_BUF_SIZE];
uint64 uart_tx_w; // 次に書き込むバイトの位置
uint64 uart_tx_r; // 次に読み取るバイトの位置

extern volatile int panicked; // printf.cから

void uartstart();

// UARTの初期化
void
uartinit(void)
{
  // 割り込みを無効にする
  WriteReg(IER, 0x00);

  // ボーレート設定モード
  WriteReg(LCR, LCR_BAUD_LATCH);

  // ボーレート38400のLSB
  WriteReg(0, 0x03);

  // ボーレート38400のMSB
  WriteReg(1, 0x00);

  // ボーレート設定モードを解除し、ワード長を8ビット、パリティなしに設定
  WriteReg(LCR, LCR_EIGHT_BITS);

  // FIFOをリセットして有効にする
  WriteReg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);

  // 送信と受信の割り込みを有効にする
  WriteReg(IER, IER_TX_ENABLE | IER_RX_ENABLE);

  initlock(&uart_tx_lock, "uart");
}

// 文字を出力バッファに追加し、UARTが送信中でなければ送信を開始する。
// 出力バッファが満杯の場合はブロックする。
// ブロックする可能性があるため、割り込みから呼び出すことはできず、write()によってのみ使用される。
void
uartputc(int c)
{
  acquire(&uart_tx_lock);

  if(panicked){
    for(;;)
      ;
  }
  while(uart_tx_w == uart_tx_r + UART_TX_BUF_SIZE){
    // バッファが満杯
    // uartstart()がバッファの空きを作るのを待つ
    sleep(&uart_tx_r, &uart_tx_lock);
  }
  uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE] = c;
  uart_tx_w += 1;
  uartstart();
  release(&uart_tx_lock);
}

// 割り込みを使用しないuartputc()の別バージョン。
// カーネルのprintf()や文字のエコーに使用される。
// UARTの出力レジスタが空になるのを待つ。
void
uartputc_sync(int c)
{
  push_off();

  if(panicked){
    for(;;)
      ;
  }

  // LSRのTransmit Holding Emptyがセットされるのを待つ
  while((ReadReg(LSR) & LSR_TX_IDLE) == 0)
    ;
  WriteReg(THR, c);

  pop_off();
}

// UARTがアイドル状態で、送信バッファに文字がある場合は送信する。
// 呼び出し元はuart_tx_lockを保持している必要がある。
// トップハーフとボトムハーフの両方から呼び出される。
void
uartstart()
{
  while(1){
    if(uart_tx_w == uart_tx_r){
      // 送信バッファが空
      return;
    }

    if((ReadReg(LSR) & LSR_TX_IDLE) == 0){
      // UART送信保持レジスタが満杯なので、次のバイトを送信できない。
      // 準備ができたら割り込みが発生する。
      return;
    }

    int c = uart_tx_buf[uart_tx_r % UART_TX_BUF_SIZE];
    uart_tx_r += 1;

    // バッファの空きを待っているかもしれないuartputc()を起こす
    wakeup(&uart_tx_r);

    WriteReg(THR, c);
  }
}

// UARTから1文字の入力を読み取る。
// 入力待ちがない場合は-1を返す。
int
uartgetc(void)
{
  if(ReadReg(LSR) & 0x01){
    // 入力データが準備完了
    return ReadReg(RHR);
  } else {
    return -1;
  }
}

// 入力が到着した、またはUARTが新しい出力を受け付ける準備ができたために発生するUART割り込みを処理する。
// devintr()から呼び出される。
void
uartintr(void)
{
  // 受信文字を読み取り処理する。
  while(1){
    int c = uartgetc();
    if(c == -1)
      break;
    consoleintr(c);
  }

  // バッファされた文字を送信する。
  acquire(&uart_tx_lock);
  uartstart();
  release(&uart_tx_lock);
}
