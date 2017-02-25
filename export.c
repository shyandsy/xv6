#include "types.h"
#include "stat.h"
#include "user.h"

static char global_variable_path[4096]; // the PATH variable

int
main(int argc, char *argv[])
{
  int length = 0;

  //make sure 2 arguments
  if(argc != 3){
    printf(1, "The export command has 2 arguments. %d given. \n", argc-1);
    exit();
  }

  //the first one must be PATH
  if(strcmp("PATH", argv[1]) != 0){
    printf(1, "The first argument must be PATH. %s given. \n", argv[0]);
    exit();
  }

  length = strlen(argv[2]);
  //printf(1, "length is %d\n", length);
  
  if(length > 4095){
    printf(1, "The length cannot exceed 4095.\n");
    exit();
  }

  memset(global_variable_path, 0, 4096*sizeof(char));
  memmove(global_variable_path, argv[2], length * sizeof(char));
  global_variable_path[length] = '\0';

  //printf(1, "path is: %s\n", global_variable_path);

  //set the PATH
  set_global_path(global_variable_path);

  exit();
}
