#include "kernel/types.h"
#include "user/user.h"

int main() {
  int fd[2];
  char buf[20];
  
  pipe(fd);
  
  int pid = fork();
  if(pid == 0) {
    close(fd[0]);
    write(fd[1], "Hello via pipe!", 15);
    close(fd[1]);
    exit(0);
  } else {
    close(fd[1]);
    read(fd[0], buf, 15);
    write(1, buf, 15);
    write(1, "\n", 1);
    wait(0);
  }
  exit(0);
}
