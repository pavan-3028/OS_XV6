#include "types.h"
#include "stat.h"
#include "user.h"
#include "signal.h"
void handler() {
  printf(1, "Received SIGUSR1!\n");
}

int main() {
  signal(SIGUSR1, handler);

  int pid = fork();

  if(pid == 0){
    while(1);  // wait
  } else {
    sleep(10);
    kill(pid, SIGUSR1);
  }

  wait();
  exit();
}
