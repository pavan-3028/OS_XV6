#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
      return -1;
    if(addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
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

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}


// Message passing system calls
extern struct proc proc[];

uint64
sys_msgsend(void)
{
  int pid, size;
  char buf[256];
  struct proc *p;

  argint(0, &pid);
  argint(2, &size);
  if(argstr(1, buf, sizeof(buf)) < 0)
    return -1;

  // Fix 1: force null termination
  buf[255] = 0;
  // Fix 2: use actual string length
  size = strlen(buf) + 1;

  for(p = proc; p < &proc[NPROC]; p++){
    if(p->pid == pid){
      for(int i = 0; i < 8; i++){
        if(p->msgbox[i].used == 0){
          p->msgbox[i].used = 1;
          p->msgbox[i].sender_pid = myproc()->pid;
          p->msgbox[i].size = size;
          memmove(p->msgbox[i].data, buf, size);
          // Fix 3: null terminate stored message
          p->msgbox[i].data[size-1] = 0;
          return 0;
        }
      }
      return -1;
    }
  }
  return -1;
}

uint64
sys_msgrecv(void)
{
  int size;
  uint64 buf;
  struct proc *p = myproc();

  argaddr(0, &buf);
  argint(1, &size);

  for(int i = 0; i < 8; i++){
    if(p->msgbox[i].used == 1){
      int sz = p->msgbox[i].size < size ? p->msgbox[i].size : size;
      // Fix: always null terminate output
      p->msgbox[i].data[sz-1] = 0;
      if(copyout(p->pagetable, buf, p->msgbox[i].data, sz) < 0)
        return -1;
      p->msgbox[i].used = 0;
      return p->msgbox[i].sender_pid;
    }
  }
  return -1;
}
