#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"


int
main(int argc, char* argv[]) {
  int i;
  char *execargs[MAXARG];
  char arg[64];
  char c;
  int pid;
  for(i=1;i<argc;i++){
    //printf("%d: %s\n",i,argv[i]);
    execargs[i-1] = argv[i];
  }
  execargs[argc-1] = arg;
  int j=0;
  while(read(0, &c, 1)){
    
    if(c == '\0' || c == '\n'){
      execargs[argc-1][j++] = '\0';
      //printf("arg: %s\n", execargs[argc-1]);
      pid = fork();
      if(pid == 0){
        exec(execargs[0], execargs);
      }
      wait(0);
      if(c == '\0')
        break;
    }
    else{
      execargs[argc-1][j++] = c;
    }
    
  }
  exit(0);
}