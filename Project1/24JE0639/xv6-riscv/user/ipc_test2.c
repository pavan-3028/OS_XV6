#include "kernel/types.h"
#include "user/user.h"

int main() {
    int shmid = shmcreate(10);
    char* shm_address = shmat(shmid);

    // Just busy wait - no sleep needed
    while (shm_address[0] != 1) {
        // do nothing, keep checking
    }

    printf("This is the message from the first user: ");
    for (int i = 1; ; i++) {
        if (shm_address[i] == '\0') break;
        printf("%c", shm_address[i]);
    }
    printf("\n");

    exit(0);
}
