#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char* argv[]) {
  int p[2];
  char str[1];
  int pid;
  if (pipe(p) < 0) {
    exit(-1);
  }
  pid = fork();
  if (pid < 0) {
    exit(-1);
  }
  if (pid == 0) {
    read(p[0], str, 1);
    printf("%d: received ping\n", getpid());
    write(p[1], "a", 1);
    close(p[0]);
    close(p[1]);
    exit(0);
  }
  write(p[1], "a", 1);
  wait(0);
  read(p[0], str, 1);
  printf("%d: received pong\n", getpid());
  close(p[0]);
  close(p[1]);
  exit(0);
}