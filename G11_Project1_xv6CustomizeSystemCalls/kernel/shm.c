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
