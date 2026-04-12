#include "kernel/types.h"
#include "user/user.h"

int main() {
  int pipe1[2], pipe2[2];
  char buf[50];

  // Two pipes for bidirectional communication
  pipe(pipe1);  // parent writes, child reads
  pipe(pipe2);  // child writes, parent reads

  int pid = fork();

  if(pid == 0) {
    // CHILD
    close(pipe1[1]);
    close(pipe2[0]);

    read(pipe1[0], buf, 20);
    printf("Child received: %s\n", buf);

    write(pipe2[1], "Hi Parent!", 10);
    close(pipe1[0]);
    close(pipe2[1]);
    exit(0);

  } else {
    // PARENT
    close(pipe1[0]);
    close(pipe2[1]);

    write(pipe1[1], "Hi Child!", 9);
    close(pipe1[1]);

    read(pipe2[0], buf, 20);
    printf("Parent received: %s\n", buf);
    close(pipe2[0]);
    wait(0);
  }
  exit(0);
}
