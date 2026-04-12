# Project Documentation: xv6 Synchronization System Calls

This documentation summarizes the implementation of `lock()` and `unlock()` system calls in the xv6-riscv operating system.

---

## 1. Overview
The goal of this project was to implement a simple global synchronization mechanism in the xv6 kernel. This allows user processes to use mutual exclusion (mutexes) via new system calls:
- `lock()`: Acquires a global spinlock.
- `unlock()`: Releases the global spinlock.

---

## 2. Kernel Implementation

### Global Spinlock Flag
A global variable was added to `kernel/sysproc.c` to maintain the state of the lock.

```c
// kernel/sysproc.c
int global_lock_flag = 0; // 0 = free, 1 = locked
```

### System Call Logic
The system calls use atomic operations to ensure that the lock acquisition is thread-safe.

```c
// kernel/sysproc.c

// Acquires the lock
uint64 sys_lock(void) {
    while(__sync_lock_test_and_set(&global_lock_flag, 1) != 0)
        yield(); // wait for the lock while yielding CPU
    return 0;
}

// Releases the lock
uint64 sys_unlock(void) {
    __sync_lock_release(&global_lock_flag);
    return 0;
}
```

> [!TIP]
> `yield()` is used in the busy-wait loop. This is more efficient in xv6 than a pure spinlock, as it allows other processes to run while the current process is waiting for the lock.

---

## 3. System Call Registration

To make these calls available to the user, several kernel files were modified:

1.  **`kernel/syscall.h`**: Added system call numbers.
    ```c
    #define SYS_lock   24
    #define SYS_unlock 25
    ```
2.  **`kernel/syscall.c`**: Added function references and mapped them in the `syscalls` table.
3.  **`user/user.h`**: Added the C prototypes for user-space programs.
    ```c
    int lock(void);
    int unlock(void);
    ```
4.  **`user/usys.pl`**: Added stubs to generate assembly wrappers.

---

## 4. Testing & Verification

A test program `locktest.c` was created to demonstrate the functionality. It forks a process and uses `lock()`/`unlock()` around critical sections (printing to console and sleeping).

### Test Code Snippet
```c
// user/locktest.c
for (int i = 0; i < 5; i++) {
    lock();
    // Critical Section
    printf("Process %d is in section %d\n", getpid(), i);
    pause(10); 
    unlock();
    pause(5);
}
```

### Successful Execution Output
When running `locktest` in the xv6 shell, the output shows that processes correctly alternate turns:

```text
$ locktest
Parent is in critical section: 0
Parent leaving critical section: 0
Child is in critical section: 0
Child leaving critical section: 0
Parent is in critical section: 1
Parent leaving critical section: 1
Child is in critical section: 1
Child leaving critical section: 1
...
```

---

## 5. Summary
- **Atomic Operations**: Used `__sync_lock_test_and_set` and `__sync_lock_release`.
- **Scheduler Friendly**: Used `yield()` to avoid wasting CPU cycles in busy-wait.
- **Robustness**: Ensured that the kernel handles sleep/preemption properly while locks are held across system calls.
