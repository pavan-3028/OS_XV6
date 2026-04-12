#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main() {
  int pid;
  char msg[] = "Hello from parent!";
  char buf[256];

  printf("=== Message Passing Test ===\n");

  pid = fork();

  if(pid < 0){
    printf("Fork failed\n");
    exit(1);
  }

  if(pid == 0) {
    // Child process

    int sender;
    while((sender = msgrecv(buf, sizeof(buf))) < 0);

    printf("Child (PID %d): got message from PID %d: %s\n",
           getpid(), sender, buf);

    exit(0);

  } else {
    // Parent process

    int r = msgsend(pid, msg, strlen(msg) + 1);

    wait(0);  // wait FIRST → ensures child prints first

    if(r < 0){
      printf("Parent (PID %d): msgsend failed\n", getpid());
    } else {
      printf("Parent (PID %d): sent message to child (PID %d)\n",
             getpid(), pid);
    }

    printf("=== Test Complete ===\n");
    exit(0);
  }
}
