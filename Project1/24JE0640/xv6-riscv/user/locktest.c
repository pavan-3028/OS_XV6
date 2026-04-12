#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main() {
    int pid = fork();

    if (pid < 0) {
        printf("fork failed\n");
        exit(1);
    }

    for (int i = 0; i < 5; i++) {
        lock();
        if (pid == 0) {
            printf("Child is in critical section: %d\n", i);
            pause(10);
            printf("Child leaving critical section: %d\n", i);
        } else {
            printf("Parent is in critical section: %d\n", i);
            pause(10);
            printf("Parent leaving critical section: %d\n", i);
        }
        unlock();
        pause(5);
    }
    
    if (pid > 0) {
        wait(0);
    }
    
    exit(0);
}
