#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

// システムコールexitの実装。
// プロセスを終了する。
uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // ここには到達しない
}

// システムコールgetpidの実装。
// プロセスIDを返す。
uint64
sys_getpid(void)
{
  return myproc()->pid;
}

// システムコールforkの実装。
// プロセスを複製する。
uint64
sys_fork(void)
{
  return fork();
}

// システムコールwaitの実装。
// 子プロセスの終了を待つ。
uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

// システムコールsbrkの実装。
// プロセスメモリを増加させる。
uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

// システムコールsleepの実装。
// 指定されたティック数だけスリープする。
uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// システムコールkillの実装。
// 指定されたプロセスを終了させる。
uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// システムコールuptimeの実装。
// システムが開始してからのクロックティック数を返す。
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
