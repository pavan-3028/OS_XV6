#include "types.h"
#include "stat.h"
#include "user.h"
#include "signal.h"

int main(){
  int pid = fork();

  if(pid == 0){
    while(1);   // child waits
  } else {
    sleep(50);
    printf(1, "Sending message signal...\n");
    killmsg(pid, SIGUSR1, "Hi Child!");
  }

  wait();
  exit();
}
