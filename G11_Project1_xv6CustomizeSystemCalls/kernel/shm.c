#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "shm.h"

struct spinlock shm_lock;         // Protects the table itself
struct shm_table_entry shm_table[SHM_MAX];

void shminit(void) {
  initlock(&shm_lock, "shm_table");

  for (int i = 0; i < SHM_MAX; i++) {
    initlock(&shm_table[i].lock, "shm_entry");
    shm_table[i].key = 0;
    shm_table[i].pa = 0;
    shm_table[i].ref_count = 0;
  }
}

uint64 sys_shmcreate(void) {
  int key;
  argint(0, &key);

  acquire(&shm_lock);

  // Check if segment already exists
  for (int i = 0; i < SHM_MAX; i++) {
    if (shm_table[i].key == key && shm_table[i].pa != 0) {
      release(&shm_lock);
      return i; // Return shmid
    }
  }

  // Find an empty slot
  for (int i = 0; i < SHM_MAX; i++) {
    if (shm_table[i].pa == 0) {
      uint64 pa = (uint64)kalloc();
      if (pa == 0) {
        release(&shm_lock);
        return -1; // Out of memory
      }
      memset((void*)pa, 0, PGSIZE);
      shm_table[i].key = key;
      shm_table[i].pa = pa;
      shm_table[i].ref_count = 0; // It will be incremented on shmat
      release(&shm_lock);
      return i; // Return shmid
    }
  }

  release(&shm_lock);
  return -1; // Table full
}

uint64 sys_shmat(void) {
  int shmid;
  argint(0, &shmid);

  if (shmid < 0 || shmid >= SHM_MAX) return -1;

  acquire(&shm_lock);
  if (shm_table[shmid].pa == 0) {
    release(&shm_lock);
    return -1;
  }

  struct proc *p = myproc();
  uint64 va = PGROUNDUP(p->sz);

  // Map the shared physical page to the process's page table
  if (mappages(p->pagetable, va, PGSIZE, shm_table[shmid].pa, PTE_W | PTE_R | PTE_U) < 0) {
    release(&shm_lock);
    return -1;
  }

  shm_table[shmid].ref_count++;
  p->sz = va + PGSIZE; // Increase process size to include the shared page
  release(&shm_lock);

  return va; // Return the virtual address where it is attached
}

uint64 sys_shmdt(void) {
  int shmid;
  argint(0, &shmid);

  if (shmid < 0 || shmid >= SHM_MAX) return -1;

  acquire(&shm_lock);
  if (shm_table[shmid].pa == 0) {
    release(&shm_lock);
    return -1;
  }

  struct proc *p = myproc();
  uint64 va = 0;
  // Search for the virtual address where the shared page is mapped
  for (uint64 a = 0; a < p->sz; a += PGSIZE) {
    pte_t *pte = walk(p->pagetable, a, 0);
    if (pte != 0 && (*pte & PTE_V)) {
      if (PTE2PA(*pte) == shm_table[shmid].pa) {
        va = a;
        break;
      }
    }
  }

  if (va != 0) {
    uvmunmap(p->pagetable, va, 1, 0); // Unmap without freeing the underlying physical memory
    shm_table[shmid].ref_count--;
  }

  release(&shm_lock);
  return 0;
}

// Called when a process is freed to ensure shared memory is detached.
void shm_release(pagetable_t pagetable, uint64 sz) {
  acquire(&shm_lock);
  for(int i = 0; i < SHM_MAX; i++) {
    if(shm_table[i].pa != 0) {
      for (uint64 a = 0; a < sz; a += PGSIZE) {
        pte_t *pte = walk(pagetable, a, 0);
        if (pte != 0 && (*pte & PTE_V) && PTE2PA(*pte) == shm_table[i].pa) {
          uvmunmap(pagetable, a, 1, 0); // Unmap without freeing underlying pa
          shm_table[i].ref_count--;
          if (shm_table[i].ref_count <= 0) {
            kfree((void*)shm_table[i].pa);
            shm_table[i].pa = 0;
            shm_table[i].key = 0;
            shm_table[i].ref_count = 0;
          }
          break;
        }
      }
    }
  }
  release(&shm_lock);
}
