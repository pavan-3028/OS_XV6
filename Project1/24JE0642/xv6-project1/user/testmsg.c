#include "kernel/types.h"
#include "user/user.h"

int main()
{
    int pid = fork();

    if(pid == 0){
        printf("Child running\n");
        exit(0);
    } else {
        waitpid(pid);
        printf("Parent done\n");
    }

    exit(0);
}