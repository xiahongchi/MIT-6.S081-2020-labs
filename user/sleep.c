#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if (argc == 1){
    write(1, "Please input the number of ticks to sleep", strlen("Please input the number of ticks to sleep"));
    exit(-1);
  }
  if(sleep(atoi(argv[1])) < 0){
    exit(-1);
  }
  exit(0);
}