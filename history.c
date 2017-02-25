#include "types.h"
#include "stat.h"
#include "user.h"

#define MAX_HISTORY                 (16)      /*the max number of the comand histories*/
#define MAX_COMMAND_LENGTH          (128)     /*the max length of the comand*/

int
main(int argc, char *argv[])
{
    char buffer[MAX_COMMAND_LENGTH];
    int ret = 0;
    for(int i = 0; i < MAX_HISTORY && ret==0; i++){
        memset(buffer, 0, 128 * sizeof(char));

        ret = history(buffer, i);
        
        if(ret == 0){
            printf(1, "%s\n", buffer);
        }
    }

    exit();
}
