#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

extern struct proc proc[];

// Create a message box for the calling process with the given name.
// Returns 0 on success, -1 on error.
int
msgbox_create_impl(char *name)
{
  struct proc *p = myproc();
  struct proc *pp;

  if(name[0] == '\0')
    return -1;

  // Check that this process doesn't already have an active mailbox
  acquire(&p->mbox.lock);
  if(p->mbox.active){
    release(&p->mbox.lock);
    return -1;
  }
  release(&p->mbox.lock);

  // Check that no other process already uses this name
  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp == p)
      continue;
    acquire(&pp->mbox.lock);
    if(pp->mbox.active && strncmp(pp->mbox.name, name, MSGBOX_NAME) == 0){
      release(&pp->mbox.lock);
      return -1;
    }
    release(&pp->mbox.lock);
  }

  acquire(&p->mbox.lock);
  strncpy(p->mbox.name, name, MSGBOX_NAME);
  p->mbox.head = 0;
  p->mbox.tail = 0;
  p->mbox.count = 0;
  p->mbox.active = 1;
  release(&p->mbox.lock);

  return 0;
}

// Send a message to the mailbox with the given name.
// Blocks if the target mailbox is full. Returns 0 on success, -1 on error.
int
msgbox_send_impl(char *name, char *data, int len)
{
  struct proc *target = 0;
  struct proc *pp;

  if(len <= 0 || len > MSG_SIZE)
    return -1;

  // Find the target process by mailbox name
  for(pp = proc; pp < &proc[NPROC]; pp++){
    acquire(&pp->mbox.lock);
    if(pp->mbox.active && strncmp(pp->mbox.name, name, MSGBOX_NAME) == 0){
      target = pp;
      // Keep lock held
      break;
    }
    release(&pp->mbox.lock);
  }

  if(target == 0)
    return -1;

  // Wait until there is space in the mailbox
  while(target->mbox.count == MSG_SLOTS){
    // Release mailbox lock before sleeping
    release(&target->mbox.lock);

    // Check if we've been killed
    if(killed(myproc()))
      return -1;

    // Sleep on the target mailbox address (senders sleep here)
    acquire(&target->mbox.lock);
    if(target->mbox.count == MSG_SLOTS){
      // Need to sleep — use sleep() with the mailbox lock
      // We use the address of mbox.count as the channel for senders
      sleep(&target->mbox.tail, &target->mbox.lock);
      // After wakeup, lock is re-acquired by sleep
    }
    // Check if mailbox is still active
    if(!target->mbox.active){
      release(&target->mbox.lock);
      return -1;
    }
  }

  // Copy message into the next slot
  memmove(target->mbox.msgs[target->mbox.tail].data, data, len);
  target->mbox.msgs[target->mbox.tail].len = len;
  target->mbox.tail = (target->mbox.tail + 1) % MSG_SLOTS;
  target->mbox.count++;

  // Wake up receiver if it was waiting for a message
  wakeup(&target->mbox.head);

  release(&target->mbox.lock);
  return 0;
}

// Receive a message from the calling process's own mailbox.
// Blocks if empty. Returns number of bytes read, or -1 on error.
int
msgbox_recv_impl(char *data, int maxlen)
{
  struct proc *p = myproc();
  int len;

  if(maxlen <= 0)
    return -1;

  acquire(&p->mbox.lock);

  if(!p->mbox.active){
    release(&p->mbox.lock);
    return -1;
  }

  // Wait until there is a message
  while(p->mbox.count == 0){
    if(killed(p)){
      release(&p->mbox.lock);
      return -1;
    }
    // Sleep on the head pointer (receivers sleep here)
    sleep(&p->mbox.head, &p->mbox.lock);
    // After wakeup, lock is re-acquired
    if(!p->mbox.active){
      release(&p->mbox.lock);
      return -1;
    }
  }

  // Dequeue the oldest message
  len = p->mbox.msgs[p->mbox.head].len;
  if(len > maxlen)
    len = maxlen;
  memmove(data, p->mbox.msgs[p->mbox.head].data, len);
  p->mbox.head = (p->mbox.head + 1) % MSG_SLOTS;
  p->mbox.count--;

  // Wake up any senders blocked on a full mailbox
  wakeup(&p->mbox.tail);

  release(&p->mbox.lock);
  return len;
}

// Destroy the calling process's message box.
// Returns 0 on success, -1 if no mailbox exists.
int
msgbox_destroy_impl(void)
{
  struct proc *p = myproc();

  acquire(&p->mbox.lock);
  if(!p->mbox.active){
    release(&p->mbox.lock);
    return -1;
  }

  p->mbox.active = 0;
  p->mbox.head = 0;
  p->mbox.tail = 0;
  p->mbox.count = 0;
  memset(p->mbox.name, 0, MSGBOX_NAME);

  // Wake up anyone sleeping on this mailbox (senders or receivers)
  wakeup(&p->mbox.head);
  wakeup(&p->mbox.tail);

  release(&p->mbox.lock);
  return 0;
}

// Return the number of pending messages, or -1 if no mailbox.
int
msgbox_count_impl(void)
{
  struct proc *p = myproc();
  int c;

  acquire(&p->mbox.lock);
  if(!p->mbox.active){
    release(&p->mbox.lock);
    return -1;
  }
  c = p->mbox.count;
  release(&p->mbox.lock);
  return c;
}
