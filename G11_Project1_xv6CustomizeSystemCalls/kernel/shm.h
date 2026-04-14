#ifndef SHM_H
#define SHM_H

#include "types.h"
#include "spinlock.h"

// Maximum number of shared memory segments
#define SHM_MAX 64

struct shm_table_entry {
  int key;               // Unique key to identify the shared memory segment
  uint64 pa;             // Physical address of the allocated page
  int ref_count;         // Number of processes currently attached
  struct spinlock lock;  // Protects this specific table entry
};

void shminit(void);

#endif // SHM_H
