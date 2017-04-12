#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

typedef struct _pqueue_t
{
    int size;                           /*the number of element inside*/
    int capacity;                       /*the total size of the priority queue*/
    struct proc* procs[NPROC]; /*memory to store void**/
} pqueue_t;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

pqueue_t queue1;  // priority queue for priority 1
pqueue_t queue2;  // priority queue for priority 2
pqueue_t queue3;  // priority queue for priority 3

/****************************************************/
// create a priority queuepqueue_t
void pqueue_create(pqueue_t* q, int cap){
    q->size = 0;
    q->capacity = cap;
    for(int i=0; i < cap; i++)
      q->procs[i] = 0;
}

// insert an element into the queue
void pqueue_enque(pqueue_t* q, struct proc* p){
    int pos;
    if(q->size == q->capacity){
        panic("error: the priority queue is full");
    }else{
        //find a position from last element of the array
        pos = q->size - 1;
        while(pos >= 0 && q->procs[pos]->creation_time > p->creation_time){
            q->procs[pos+1] = q->procs[pos];
            pos --;
        }

        //insert into the arry
        q->procs[pos+1] = p;
        q->size ++;
    }
}

// remove an element into the queue
//struct proc* pqueue_deque(pqueue_t* q);

// check the first an element into the queue
struct proc* pqueue_peek(pqueue_t* q){
    struct proc* np = 0;
    int pos = 0;

    for(pos = 0; pos < q->size; pos++){
      if(q->procs[pos]->state == RUNNABLE){
        np = q->procs[pos];
        break;
      }
    }

    return np;
}

int pqueue_size(pqueue_t* q){
    return q->size;
}

void pqueue_remove(pqueue_t* q, struct proc* proc){
    int pos;
    bool found = false;
    
    // find the pos of the element
    pos = 0;
    while(pos < q->size && !found){
        if(q->procs[pos] == proc){
            found = true;
        }else{
            pos++;
        }
    }
    if(found){
        while(pos < q->size-1){
            q->procs[pos] = q->procs[pos+1];
            pos++;
        }
        q->procs[q->size-1] = 0;
        q->size --;
    }

}
/****************************************************/

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");

  // create three queue
  pqueue_create(&queue1, NPROC);
  pqueue_create(&queue2, NPROC);
  pqueue_create(&queue3, NPROC);
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;
  
  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;
  
  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  //initialize the trap handlers to 1
  p->handlers[0] = (sighandler_t)-1;
  p->handlers[1] = (sighandler_t)-1;

	p->ctime = ticks;
	p->stime = 0;
	p->retime = 0;
	p->rutime = 0;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  
  p = allocproc();
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  p->creation_time = ticks;  //creation time
  
  #ifdef SML
  /**** initial the process to be priority 2 : SML ****/
  p->priority = 2;
  pqueue_enque(&queue2, p);
  /****************************************************/
  #endif

  #ifdef DML
  /**** initial the process to be priority 2 : DML ****/
  p->priority = 2;
  pqueue_enque(&queue2, p);
  /****************************************************/
  #endif

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
  p->retime = ticks;
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  
  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  #ifdef FCFS
  /**** initial the process to be priority 2 : FCFS ****/
  np->creation_time = ticks;  // creation time
  /****************************************************/
  #endif

  #ifdef SML
  /**** initial the process to be priority 3 : SML ****/
  np->creation_time = ticks; // copy the parent priority
  np->priority = proc->priority;
  if(np->priority == 1){
    pqueue_enque(&queue1, np);
  }else if(np->priority == 2){
    pqueue_enque(&queue2, np);
  }else if(np->priority == 3){
    pqueue_enque(&queue3, np);
  }else{
    panic("priority can be 123 only");
  }
  /****************************************************/
  #endif

  #ifdef DML
  /**** initial the process to be priority 3 : DML ****/
  np->creation_time = ticks; // copy the parent priority
  np->priority = proc->priority;
  if(np->priority == 1){
    pqueue_enque(&queue1, np);
  }else if(np->priority == 2){
    pqueue_enque(&queue2, np);
  }else if(np->priority == 3){
    pqueue_enque(&queue3, np);
  }else{
    panic("priority can be 123 only");
  }
  /****************************************************/
  #endif

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack =
     0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;
  
  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);
 
  pid = np->pid;
  np->state = RUNNABLE;

  np->retime = ticks;
  safestrcpy(np->name, proc->name, sizeof(proc->name));
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  iput(proc->cwd);
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        
        #ifdef SML
        /**** remove from priority queue : SML ****/
        //remove from queue
        if(p->priority == 1){
          pqueue_remove(&queue1, p);
        }else if(p->priority == 2){
          pqueue_remove(&queue2, p);
        }else if(p->priority == 3){
          pqueue_remove(&queue3, p);
        }
        /****************************************************/
        #endif 

        #ifdef DML
        /**** remove from priority queue : DML ****/
        //remove from queue
        if(p->priority == 1){
          pqueue_remove(&queue1, p);
        }else if(p->priority == 2){
          pqueue_remove(&queue2, p);
        }else if(p->priority == 3){
          pqueue_remove(&queue3, p);
        }
        /****************************************************/
        #endif 

        
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;

        p->creation_time = 0; // reset creation time to 0
      
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p = 0;

  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);

    /*******************************************************/
    //default scheduler
    #ifdef DEFAULT
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      proc = p;
      switchuvm(p);
      p->state = RUNNING;
      p->rutime = ticks;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    #endif
 
    /*******************************************************/
 
    //FCFS
    #ifdef FCFS
    struct proc* firstcome_proc = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      
      // must runable
      if(p->state != RUNNABLE)
        continue;

      // find the first come process
      if(firstcome_proc == 0){
        firstcome_proc = p;
      }else if(p->creation_time < firstcome_proc->creation_time){
        firstcome_proc = p;
      }
    }

    if(firstcome_proc != 0){
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      proc = firstcome_proc;
      switchuvm(firstcome_proc);
      firstcome_proc->state = RUNNING;
      firstcome_proc->rutime = ticks;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();
    }

    // Process is done running for now.
    // It should have changed its p->state before coming back.
    proc = 0;

    #endif
    /*******************************************************/

    //SMLint
    #ifdef SML
    
    // choose from the priority queue 3
    p = pqueue_peek(&queue3);

    // choose from the priority queue 2
    if(p == 0)
      p = pqueue_peek(&queue2);

    // choose from the priority queue 1
    if(p == 0)
      p = pqueue_peek(&queue1);

    if(p != 0){
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      proc = p;
      switchuvm(p);
      p->state = RUNNING;
      p->rutime = ticks;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();
    }

    // Process is done running for now.
    // It should have changed its p->state before coming back.
    proc = 0;

    #endif
    /*******************************************************/

    //DML
    #ifdef DML

    static struct proc *lastp = 0;
    static uint counter = 0;

    counter ++;
    
    // sheduler only works on counter % QUANTA == 0
    if(counter % QUANTA == 0){
      // choose from the priority queue 3
      p = pqueue_peek(&queue3);

      // choose from the priority queue 2
      if(p == 0)
        p = pqueue_peek(&queue2);

      // choose from the priority queue 1
      if(p == 0)
        p = pqueue_peek(&queue1);

      if(p != 0){
        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        proc = p;
        switchuvm(p);
        p->state = RUNNING;
        p->rutime = ticks;
        swtch(&cpu->scheduler, proc->context);
        switchkvm();
      }

      // save current processor
      lastp = p;
    }else{
      if(lastp != 0 && lastp->state == RUNNABLE){
        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to->state us.
        proc = lastp;
        switchuvm(lastp);
        p->state = RUNNING;
        p->rutime = ticks;
        swtch(&cpu->scheduler, proc->context);
        switchkvm();
      } 
    } 

    // Process is done running for now.
    // It should have changed its p->state before coming back.
    proc = 0;

    #endif
    /*******************************************************/

    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  proc->retime = ticks;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot 
    // be run from main().
    first = 0;
    initlog();
  }
  
  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  	proc->stime = ticks;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan){
      
      /*********  DML  ***********/
      #ifdef DML
      //put it to hightest queue
      if(p->priority == 1){
        pqueue_remove(&queue1, p);
        pqueue_enque(&queue3, p);
      }else if(p->priority == 2){
        pqueue_remove(&queue2, p);
        pqueue_enque(&queue3, p);
      }
      p->priority = 3;
      #endif
      /******************************/

      p->state = RUNNABLE;
      p->retime = ticks;
    }
      
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING){
        p->state = RUNNABLE;
        p->retime = ticks;
      }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

// set priority
int sys_set_prio(void){
  int priority;

  // only works in SML
  #ifndef SML
  return 1;
  #endif

  // get parameter
  if(argint(0, &priority) < 0)
    return 1;
  
  acquire(&ptable.lock);
  switch(priority){
    case 1:
      if(proc->priority == 2){
        pqueue_remove(&queue2, proc);
        pqueue_enque(&queue1, proc);
      }else if(proc->priority == 3){
        pqueue_remove(&queue3, proc);
        pqueue_enque(&queue1, proc);
      }
      proc->priority = 1;
      break;
    case 2:
      if(proc->priority == 1){
        pqueue_remove(&queue1, proc);
        pqueue_enque(&queue2, proc);
      }else if(proc->priority == 3){
        pqueue_remove(&queue3, proc);
        pqueue_enque(&queue2, proc);
      }
      proc->priority = 2;
      break;
    case 3:
      if(proc->priority == 1){
        pqueue_remove(&queue1, proc);
        pqueue_enque(&queue3, proc);
      }else if(proc->priority == 2){
        pqueue_remove(&queue2, proc);
        pqueue_enque(&queue3, proc);
      }
      proc->priority = 3;
      break;
    default:
      return 1;
      break;
  }
  release(&ptable.lock);

  return 0;
}

int sys_yield(void){
  yield();
  return 0;
}

// reset current process priority to 2
void reset_priority(){
  acquire(&ptable.lock);
  if(proc->priority == 1){
    pqueue_remove(&queue1, proc);
    pqueue_enque(&queue2, proc);
  }else if(proc->priority == 3){
    pqueue_remove(&queue3, proc);
    pqueue_enque(&queue2, proc);
  }
  proc->priority = 2;
  release(&ptable.lock);
}

int
wait2(int *retime, int *rutime, int *stime)
{
	struct proc *p;
	int havekids, pid;

	acquire(&ptable.lock);
	for (;;) {
		// Scan through table looking for exited children.
		havekids = 0;
		for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
			if (p->parent != proc)
				continue;
			havekids = 1;
			if (p->state == ZOMBIE) {

				*retime = p-> retime;
				*rutime = p->rutime;
				*stime = p->stime;

				// Found one.
				pid = p->pid;
				kfree(p->kstack);
				p->kstack = 0;
				freevm(p->pgdir);
				p->pid = 0;
				p->parent = 0;
				p->name[0] = 0;
				p->killed = 0;
				p->state = UNUSED;
				release(&ptable.lock);
				return pid;
			}
		}

		// No point waiting if we don't have any children.
		if (!havekids || proc->killed) {
			release(&ptable.lock);
			return -1;
		}

		// Wait for children to exit.  (See wakeup1 call in proc_exit.)
		sleep(proc, &ptable.lock);  //DOC: wait-sleep
	}
}