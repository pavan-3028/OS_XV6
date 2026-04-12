#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
extern struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}
int sys_kill(void) {
  int pid, signum;

  if(argint(0, &pid) < 0 || argint(1, &signum) < 0)
    return -1;

  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->signal = signum;

      if(signum == SIGKILL)
        p->killed = 1;

      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

 int sys_killmsg(void){
  int pid, signum;
  char *msg;

  if(argint(0, &pid) < 0 || argint(1, &signum) < 0)
    return -1;

  // simplified version (fixed message)
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->signal = signum;
      safestrcpy(p->msg, "Hello from parent", sizeof(p->msg));
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}


int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
int sys_signal(void) {
  int signum;
  int handler;

  if(argint(0, &signum) < 0 || argint(1, &handler) < 0)
    return -1;

  struct proc *p = myproc();
 
  p->handler = (void*)handler;

  return 0;
}
int sys_alarm(void){
  int ticks;
  if(argint(0, &ticks) < 0)
    return -1;

  myproc()->alarmticks = ticks;
  return 0;
}
