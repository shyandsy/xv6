#include "types.h"
#include "user.h"

int main(int argc, char *argv[]){
  int pid;

  #ifndef SML
  printf(1, "it only works on SML");
  return 0;
  #endif

  pid = getpid();
  printf(1, "parent proccss pid=%d.\n", pid);

  // set priority to 1
  set_prio(1);

  for(int i=0; i<20; i++){
    pid = fork();
    if (pid == 0) {       // child process
        // set priority to 1
        set_prio(pid % 3 + 1);
        int i=0;
        while(i<1000000)
          i++;
        
        printf(1, "processors %d finished their job\n", getpid());
        break;
    } else if (pid > 0) {  // parent process
        continue;
    } else {
        printf(1, "fork error\n");
        exit();
    }
  }

  for(int i=0; i<20; i++){
    wait();
    //printf(1, "have %d processors finished their job\n", i);
  }

  exit();
}