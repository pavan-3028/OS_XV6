#include "types.h"
#include "stat.h"
#include "user.h"
#include "signal.h"

int main() {
  printf(1, "Setting alarm...\n");

  alarm(50);   // trigger after 50 ticks

  while(1){
    // keep process running
  }

  exit();
}