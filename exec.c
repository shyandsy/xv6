#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"

#define MAX_ENVIRONMENT_PATH_LENGTH (1024)    // the max length of each path
#define MAX_PATH_NUMBER             (100)     // the max number of the paths can be save.

// its use to hold one path
typedef struct _path_item{
  char path[1024];
  int length;
}path_item;

// its use to hold all path in array
path_item environment_path_set[MAX_PATH_NUMBER];

// the file path
char file_path[MAX_ENVIRONMENT_PATH_LENGTH * 2];

/*
sys_set_global_path
set the global environment path
*/
int 
sys_set_global_path(void){
  char* strpath;
  int pos = 0;
  int i = 0;
  int length = 0;
  int count = 0;
  
  //get the pointer
  if(argstr(0, &strpath) < 0)
    return -1;

  // clear all path before
  memset(environment_path_set, 0, sizeof(path_item) * MAX_PATH_NUMBER);
  
  length = strlen(strpath);
  pos = 0;
  count = 0;
  for(i = 0; i < length; i++){
    if(strpath[i] == ':'){
      //finishe
      environment_path_set[count].path[pos] = '\0';
      environment_path_set[count].length = pos;
      count ++;
      pos = 0;
    }else{
      //add this char to string
      if(pos < MAX_ENVIRONMENT_PATH_LENGTH - 1){
        environment_path_set[count].path[pos] = strpath[i];
        pos++;
      }
    }
  }

  return 0;
}

int
exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint argc, sz, sp, ustack[3+MAXARG+1];
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pde_t *pgdir, *oldpgdir;
  int length;
  int found = 0;
  path_item *pitem;
  int n;
  
  //if not found, search program in the 
  if((ip = namei(path)) == 0){
    //get the length of path from parameter
    length = strlen(path);

    for(i = 0; i < MAX_PATH_NUMBER && !found; i++){
      // get current variable
      pitem = environment_path_set + i;
 
      //clear buffer
      memset(file_path, 0, sizeof(char) * MAX_ENVIRONMENT_PATH_LENGTH * 2);

      n = 0;

      //copy the environment path
      for(n=0; n < pitem->length; n++){
        file_path[n] = pitem->path[n];
      }

      //copy the path from parameter
      for(n=0; n < length; n++){
        file_path[pitem->length + n] = path[n];
      }

      //set string end
      file_path[pitem->length + length] = '\0';

      if((ip = namei(file_path)) != 0){
        path = file_path;
        found = 1;
      }
    }
    
    // nout found
    if(!found)
      return -1;
  }

  #ifdef DML    //reset the priority to 2
  reset_priority();
  #endif

  ilock(ip);
  pgdir = 0;

  // Check ELF header
  if(readi(ip, (char*)&elf, 0, sizeof(elf)) < sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  if((pgdir = setupkvm()) == 0)
    goto bad;

  // Load program into memory.
  sz = 0;
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    if(loaduvm(pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }

  iunlockput(ip);
  ip = 0;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  sz = PGROUNDUP(sz);
  if((sz = allocuvm(pgdir, sz, sz + 2*PGSIZE)) == 0)
    goto bad;
  clearpteu(pgdir, (char*)(sz - 2*PGSIZE));
  sp = sz;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
    if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[3+argc] = sp;
  }
  ustack[3+argc] = 0;

  ustack[0] = 0xffffffff;  // fake return PC
  ustack[1] = argc;
  ustack[2] = sp - (argc+1)*4;  // argv pointer

  sp -= (3+argc+1) * 4;
  if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)
    goto bad;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(proc->name, last, sizeof(proc->name));

  // Commit to the user image.
  oldpgdir = proc->pgdir;
  proc->pgdir = pgdir;
  proc->sz = sz;
  proc->tf->eip = elf.entry;  // main
  proc->tf->esp = sp;
  switchuvm(proc);
  freevm(oldpgdir);
  return 0;

 bad:
  if(pgdir)
    freevm(pgdir);
  if(ip)
    iunlockput(ip);
  return -1;
}
