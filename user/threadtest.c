#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/syscall.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"

void thread_a(void) {
  printf("something\n");
  exit(0);
}

void some_long_proc() {
  for (int i = 0; i < 100000; i++) {
    /* some long running unnecessary loop */
  }
}

int main(int argc, char *argv[]) {
  printf("created thread id - %d\n", create_thread(thread_a));
  printf("looping\n");
  some_long_proc();
  printf("another loop\n");
  some_long_proc();
  printf("another loop\n");
  some_long_proc();
  printf("another loop\n");
  some_long_proc();
  printf("another loop\n");
  some_long_proc();
  printf("another loop\n");
  some_long_proc();
  printf("another loop\n");
  some_long_proc();

  exit(0);
}