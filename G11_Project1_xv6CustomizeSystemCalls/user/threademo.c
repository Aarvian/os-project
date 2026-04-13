#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
worker(void *arg)
{
  int id = (int)(uint64)arg;

  printf("  [Thread %d] Started\n", id);
  printf("  [Thread %d] Finished\n", id);

  exit(0);  // threads must call exit() when done
}

int main(void){
  printf("\n--- Test 3: thread_create ---\n");

  int tid1 = thread_create(worker, (void*)1);
  int tid2 = thread_create(worker, (void*)2);

  if (tid1 < 0 || tid2 < 0) {
    printf("  ERROR: thread_create failed\n");
    exit(1);
  }

  printf("  Created Thread 1 with tid=%d\n", tid1);
  printf("  Created Thread 2 with tid=%d\n", tid2);

  // ── TEST 4: thread_join ──────────────────────────────────
  printf("\n--- Test 4: thread_join ---\n");

  if (thread_join(tid1) == 0)
    printf("  Thread 1 joined successfully\n");
  else
    printf("  ERROR: thread_join(tid1) failed\n");

  if (thread_join(tid2) == 0)
    printf("  Thread 2 joined successfully\n");
  else
    printf("  ERROR: thread_join(tid2) failed\n");

  printf("\n========================================\n");
  printf("  Thread tests complete\n");
  printf("========================================\n\n");

  exit(0);
}