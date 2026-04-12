#include "user.h"
#include <stddef.h>

int main() {
    int key = 10;
    int shmid = shmcreate(key);
    int pid=fork();
    char* shm_address = shmat(shmid);
    if (pid==0){
    char buffer[20];
    printf("This is the child process!!!\n");
    printf("Enter a message to the other chat(max characters should be 20):");
    gets(buffer, 20);

    strcpy(shm_address + 1, (const char*)buffer); // write msg at offset 1
    shm_address[0] = '1';  // ← set ready flag LAST, after message is written
    printf("%c%c%c%c%c\n",shm_address[1],shm_address[2],shm_address[3],shm_address[4],shm_address[5]);
    shmdt(shm_address,key);
    }
    else {
	 wait(NULL);
	 printf("This is the parent process!!!!\n");
	 for (int i=1;;i++){
		 if (shm_address[i]=='\0') break;
		 else printf("%c",shm_address[i]);
         }
	 printf("\n");
     }
   return 0;
}
