typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;

typedef uint pde_t;

typedef int bool;
#define true (1)
#define false (0)

//signals
#define SIGFPE          (0x00)
#define SIGSEGV         (0x01)

typedef void (*sighandler_t)(void);
