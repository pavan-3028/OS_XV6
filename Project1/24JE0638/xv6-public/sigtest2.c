#include "types.h"
#include "stat.h"
#include "user.h"
#include "signal.h"

int main() {
  int pid = fork();

  if(pid == 0){
    while(1){
      printf(1, "Child running...\n");
      sleep(100);
    }
    exit();
  } else {
    sleep(200);

    printf(1, "Sending SIGSTOP\n");
    kill(pid, SIGSTOP);

    sleep(200);

    printf(1, "Sending SIGCONT\n");
    kill(pid, SIGCONT);

    sleep(200);

    printf(1, "Sending SIGUSR1\n");
    kill(pid, SIGUSR1);

    sleep(200);

    printf(1, "Sending SIGKILL\n");
    kill(pid, SIGKILL);

    wait();
    exit();
                                  }
}
