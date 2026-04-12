#include "kernel/types.h"
#include "user/user.h"

int main() {
  int fd[2];
  char buf[50];
  int n;

  // Check if pipe creation fails
  if(pipe(fd) < 0) {
    printf("ERROR: Pipe creation failed!\n");
    exit(1);
  }
  printf("Pipe created successfully!\n");

  int pid = fork();

  if(pid < 0) {
    printf("ERROR: Fork failed!\n");
    exit(1);
  }

  if(pid == 0) {
    // CHILD - writer
    close(fd[0]);

    n = write(fd[1], "Safe message!", 13);
    if(n < 0) {
      printf("ERROR: Write failed!\n");
      exit(1);
    }
    printf("Child: wrote %d bytes\n", n);
    close(fd[1]);
    exit(0);

  } else {
    // PARENT - reader
    close(fd[1]);

    n = read(fd[0], buf, 50);
    if(n < 0) {
      printf("ERROR: Read failed!\n");
      exit(1);
    }
    buf[n] = 0;
    printf("Parent: received: %s\n", buf);
    printf("Parent: read %d bytes\n", n);
    close(fd[0]);
    wait(0);
  }

  printf("Pipe closed safely!\n");
  exit(0);
}
