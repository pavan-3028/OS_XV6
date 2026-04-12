#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
#define MAX_MSGQ 10
#define MSG_SIZE 128
#define MAX_MSGS 10

struct msg {
    char data[MSG_SIZE];
};

struct msg_queue {
    int key;
    struct msg msgs[MAX_MSGS];
    int front;
    int rear;
    int count;
};

struct {
    struct msg_queue queues[MAX_MSGQ];
} msg_table;

int msg_count = 0;
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
uint64
sys_msgget(void)
{
    int key;
    argint(0, &key);

    if (msg_count == MAX_MSGQ)
        return -1;

    for (int i = 0; i < msg_count; i++) {
        if (msg_table.queues[i].key == key)
            return i;
    }

    msg_table.queues[msg_count].key = key;
    msg_table.queues[msg_count].front = 0;
    msg_table.queues[msg_count].rear = 0;
    msg_table.queues[msg_count].count = 0;

    msg_count++;
    return msg_count - 1;
}
uint64
sys_msgsend(void)
{
    int msgid;
    char buf[MSG_SIZE];

    argint(0, &msgid);
    argstr(1, buf, MSG_SIZE);

    if (msgid < 0 || msgid >= msg_count)
        return -1;

    struct msg_queue *q = &msg_table.queues[msgid];

    if (q->count == MAX_MSGS)
        return -1;

    safestrcpy(q->msgs[q->rear].data, buf, MSG_SIZE);
    q->rear = (q->rear + 1) % MAX_MSGS;
    q->count++;

    return 0;
}
uint64
sys_msgrcv(void)
{
    int msgid;
    uint64 addr;

    argint(0, &msgid);
    argaddr(1, &addr);

    if (msgid < 0 || msgid >= msg_count)
        return -1;

    struct msg_queue *q = &msg_table.queues[msgid];

    if (q->count == 0)
        return -1;

    char buf[MSG_SIZE];
    safestrcpy(buf, q->msgs[q->front].data, MSG_SIZE);

    q->front = (q->front + 1) % MAX_MSGS;
    q->count--;

    struct proc *p = myproc();

    if (copyout(p->pagetable, addr, buf, MSG_SIZE) < 0)
        return -1;

    return 0;
}
uint64
sys_waitpid(void)
{
    int pid;
    uint64 addr = 0;

    argint(0, &pid);

    // call existing wait
    int ret = kwait(addr);

    if(ret == pid)
        return ret;

    return -1;
}