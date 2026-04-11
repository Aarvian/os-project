// msgbox_demo.c — simple IPC demo between two processes

#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char buf[64];

  printf("=== IPC Demo ===\n");

  msgbox_create("box1");

  int pid = fork();
  if(pid == 0){
    // Child: send a message to parent's mailbox
    pause(1); // let parent be ready
    msgbox_send("box1", "hi from child!", 15);
    printf("[child] sent message\n");
    exit(0);
  }

  // Parent: receive the message
  int len = msgbox_recv(buf, sizeof(buf));
  printf("[parent] got: \"%s\" (%d bytes)\n", buf, len);
  printf("[parent] pending: %d\n", msgbox_count());

  wait(0);
  msgbox_destroy();
  printf("=== Done ===\n");
  exit(0);
}
