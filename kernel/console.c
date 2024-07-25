//
// UARTへのコンソール入出力である。
// 読み取りは1行ずつ行われる。
// 特殊な入力文字を実装する:
//   改行 -- 行の終わり
//   control-h -- バックスペース
//   control-u -- 行の削除
//   control-d -- ファイルの終わり
//   control-p -- プロセスリストの表示
//

#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

#define BACKSPACE 0x100
#define C(x)  ((x)-'@')  // Control-x

//
// 1文字をUARTに送信する関数である。
// printf()や入力文字のエコーに呼び出されるが、write()からは呼び出されない。
//
void
consputc(int c)
{
  if(c == BACKSPACE){
    // ユーザーがバックスペースを入力した場合、スペースで上書きする。
    uartputc_sync('\b'); uartputc_sync(' '); uartputc_sync('\b');
  } else {
    uartputc_sync(c);
  }
}

struct {
  struct spinlock lock;

  // 入力バッファ
  #define INPUT_BUF_SIZE 128
  char buf[INPUT_BUF_SIZE];
  uint r;  // 読み取りインデックス
  uint w;  // 書き込みインデックス
  uint e;  // 編集インデックス
} cons;

//
// ユーザーのwrite()呼び出しがコンソールに対して行われた場合に実行される関数である。
//
int
consolewrite(int user_src, uint64 src, int n)
{
  int i;

  for(i = 0; i < n; i++){
    char c;
    if(either_copyin(&c, user_src, src+i, 1) == -1)
      break;
    uartputc(c);
  }

  return i;
}

//
// ユーザーのread()呼び出しがコンソールに対して行われた場合に実行される関数である。
// 入力ライン全体をdstにコピーする。
// user_dstはdstがユーザーアドレスかカーネルアドレスかを示す。
//
int
consoleread(int user_dst, uint64 dst, int n)
{
  uint target;
  int c;
  char cbuf;

  target = n;
  acquire(&cons.lock);
  while(n > 0){
    // 割り込みハンドラがcons.bufferに入力を入れるまで待機する。
    while(cons.r == cons.w){
      if(killed(myproc())){
        release(&cons.lock);
        return -1;
      }
      sleep(&cons.r, &cons.lock);
    }

    c = cons.buf[cons.r++ % INPUT_BUF_SIZE];

    if(c == C('D')){  // ファイルの終わり
      if(n < target){
        // 次回のために^Dを保存して、呼び出し元が0バイトの結果を取得できるようにする。
        cons.r--;
      }
      break;
    }

    // 入力バイトをユーザースペースのバッファにコピーする。
    cbuf = c;
    if(either_copyout(user_dst, dst, &cbuf, 1) == -1)
      break;

    dst++;
    --n;

    if(c == '\n'){
      // 1行全体が到着したので、ユーザーレベルのread()に戻る。
      break;
    }
  }
  release(&cons.lock);

  return target - n;
}

//
// コンソール入力割り込みハンドラである。
// uartintr()が入力文字に対してこれを呼び出す。
// 削除/行削除処理を行い、cons.bufに追加し、
// 1行全体が到着した場合にconsoleread()を起床させる。
//
void
consoleintr(int c)
{
  acquire(&cons.lock);

  switch(c){
  case C('P'):  // プロセスリストの表示。
    procdump();
    break;
  case C('U'):  // 行の削除。
    while(cons.e != cons.w &&
          cons.buf[(cons.e-1) % INPUT_BUF_SIZE] != '\n'){
      cons.e--;
      consputc(BACKSPACE);
    }
    break;
  case C('H'): // バックスペース
  case '\x7f': // デリートキー
    if(cons.e != cons.w){
      cons.e--;
      consputc(BACKSPACE);
    }
    break;
  default:
    if(c != 0 && cons.e-cons.r < INPUT_BUF_SIZE){
      c = (c == '\r') ? '\n' : c;

      // ユーザーにエコーする。
      consputc(c);

      // consoleread()が消費するために保存する。
      cons.buf[cons.e++ % INPUT_BUF_SIZE] = c;

      if(c == '\n' || c == C('D') || cons.e-cons.r == INPUT_BUF_SIZE){
        // 1行全体（またはファイルの終わり）が到着した場合にconsoleread()を起床させる。
        cons.w = cons.e;
        wakeup(&cons.r);
      }
    }
    break;
  }

  release(&cons.lock);
}

// コンソールを初期化する関数である。
void
consoleinit(void)
{
  initlock(&cons.lock, "cons");

  uartinit();

  // readおよびwriteシステムコールをconsolereadおよびconsolewriteに接続する。
  devsw[CONSOLE].read = consoleread;
  devsw[CONSOLE].write = consolewrite;
}
