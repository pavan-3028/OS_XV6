#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
#include "shm.h"

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
sys_getppid(void)
{
  return myproc()->parent->pid;
}

uint64
sys_shmcreate(void)
{
    int key;
    argint(0, &key);

    if (shm_count == shm_max)
        return -1;

    // Check if key already exists
    for (int i = 0; i < shm_count; i++) {
        if (shm_table.pages[i].key == key) {
            return i;
        }
    }

    // Allocate physical page
    void *pa = kalloc();
    if (pa == 0)
        return -1;

    // Store in shared memory table
    shm_table.pages[shm_count].pa = pa;
    shm_table.pages[shm_count].key = key;
    shm_table.pages[shm_count].programs_attached = 0;
    shm_count++;
    return (uint64)(shm_count - 1);
}   
uint64
sys_shmat(void)
{
    struct proc* p = myproc();
    int shmid;
    argint(0, &shmid);
    if (shmid < 0 || shmid >= shm_count)
        return -1;

    uint64 pa = (uint64)shm_table.pages[shmid].pa;  // kalloc VA == PA in xv6-riscv, no conversion needed
    uint64 va = PGROUNDUP(p->sz);
    p->sz = va + PGSIZE;
    if (mappages(p->pagetable, va, PGSIZE, pa, PTE_R | PTE_W | PTE_U) != 0)
        return -1;
    shm_table.pages[shmid].programs_attached++;
    return va;
}
uint64
sys_shmdt(void)
{
    struct proc* p = myproc();
    uint64 va = p->trapframe->a0;//vertual address from the shmat
    uint64 key = p->trapframe->a1;//it is the second argument that is key of the shm created
    int i;
    for (i=0;i<shm_count;i++){
	    if (shm_table.pages[i].key==key) break;
    }
    uvmunmap(p->pagetable,va,1,0);
    shm_table.pages[i].programs_attached--;
   
    return 0;//sucessfull detach

}
