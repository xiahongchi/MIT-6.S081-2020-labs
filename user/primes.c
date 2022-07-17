#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char* argv[]) {
  int top = 35;
  int p[20][2];
  int pid;
  int i, base, num, pipenum = 0;
  if (pipe(p[pipenum]) < 0) {
    exit(-1);
  }
  pid = fork();
  while (pid == 0) {
    int one = 0, origin = pipenum;
    close(p[pipenum][1]);

    read(p[pipenum][0], (char*)(&base), 4);
    printf("prime %d\n", base);

    while (read(p[pipenum][0], (char*)(&num), 4)) {
      if (num % base) {
        if (one == 0) {
          one = 1;
          if (pipe(p[pipenum + 1]) < 0) {
            exit(-1);
          }
          pid = fork();
          if (pid == 0) {  // new process
            pipenum++;
            break;
          } else {
            close(p[pipenum + 1][0]);
          }
        }
        write(p[pipenum + 1][1], (char*)(&num), 4);
      }
    }
    if (origin == pipenum) {  // old process

      close(p[pipenum][0]);
      if (one == 1) {
        close(p[pipenum + 1][1]);
        wait(0);
      }
      exit(0);
    }
  }
  close(p[pipenum][0]);
  for (i = 2; i <= top; i++) {
    write(p[pipenum][1], (char*)(&i), 4);
  }
  close(p[pipenum][1]);
  wait(0);
  exit(0);
}