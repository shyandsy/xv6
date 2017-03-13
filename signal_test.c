#include "types.h"
#include "stat.h"
#include "user.h"


void
fpf_handler(void){
    printf(1, "---------------------------------\n");
    printf(1, "fpf handled!!!!!\n");
    printf(1, "---------------------------------\n\n");
    return;
}

void
segv_handler(void){
    printf(1, "---------------------------------\n");
    printf(1, "segv handled!!!!!\n");
    printf(1, "---------------------------------\n\n");
}

void segment_fault()
{
    int *ptr = (void*)0x8000001;
    *ptr = 1;
}

int
main(int argc, char *argv[])
{
    int a = 10;
    int b = 0;
    int c;

    printf(1, "fpf_handler: %x\n", fpf_handler);
    printf(1, "fpf_handler: %x\n", &fpf_handler);
    printf(1, "segv_handler: %x\n", segv_handler);
    printf(1, "segv_handler: %x\n", &segv_handler);

    signal(SIGFPE, (sighandler_t)fpf_handler);
    signal(SIGSEGV, (sighandler_t)segv_handler);

    //segment fault
    segment_fault();

    //fpr trap
    c = a / b;

    printf(1, "finished $d%d!\n", b, c);

    exit();
}
