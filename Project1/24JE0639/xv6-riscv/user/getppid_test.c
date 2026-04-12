#include "types.h"
#include "user.h"

int main() {
    int ppid = getppid();
    printf("This is program that uses the getppid syscall and the ppid is %d\n",ppid);
    return 0;
}

