#include "types.h" 
#include "riscv.h" 
#include "defs.h" 
#include "param.h"
#include "memlayout.h" 
#include "spinlock.h" 
#include "proc.h"

extern struct proc proc[NPROC];

uint64 sys_thread_create(void){
    struct proc *parent = myproc();

    uint64 fn = parent->trapframe->a0;
    uint64 arg = parent->trapframe->a1;

    struct proc *np;

    if((np = kallocproc()) ==0){
        return -1;
    }

    np->pagetable = parent->pagetable;
    np->sz = parent->sz;

    np->is_thread = 1;
    np->thread_parent = parent->pid;

    *(np->trapframe) = *(parent->trapframe);
    np->trapframe->epc = fn;   // thread entry point (RISC-V: epc not eip)
    np->trapframe->a0  = arg;  // argument passed in register a0

    // Inherit open files and working directory from parent
    for (int i = 0; i < NOFILE; i++)
        if (parent->ofile[i])
            np->ofile[i] = filedup(parent->ofile[i]);
    np->cwd = idup(parent->cwd);

    safestrcpy(np->name, parent->name, sizeof(parent->name));

    // kallocproc() returns with np->lock held.
    np->state = RUNNABLE;
    release(&np->lock);

    return np->pid;  // return tid to caller
}

uint64
sys_thread_join(void)
{
  struct proc *p = myproc();

  // Read tid from trapframe register a0
  int tid = (int)p->trapframe->a0;

  // Search the process table for the thread
  struct proc *target;
  int found;

  for (;;) {
    found = 0;

    // xv6-riscv: each proc has its own lock (unlike x86 ptable lock)
    for (target = proc; target < &proc[NPROC]; target++) {
      acquire(&target->lock);

      if (target->pid == tid && target->is_thread) {
        found = 1;

        if (target->state == ZOMBIE) {
          // Thread finished — reap it
          kfreeproc(target);         // frees kstack, trapframe, etc.
          release(&target->lock);
          return 0;
        }
      }
      release(&target->lock);
    }

    if (!found)
      return -1;  // no thread with that TID exists

    // Thread exists but hasn't exited yet — yield and retry
    yield();
  }
}