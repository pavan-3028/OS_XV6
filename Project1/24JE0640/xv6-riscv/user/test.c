#include "kernel/types.h"
#include "user/user.h"

int main(){
    int year = getyear();
    printf("Year: %d\n", year);
    exit(0);
}
