#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/syscall.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"

void thread_a() {
    printf("something\n");
}

int main(int argc, char *argv[]) {
  printf("created thread id - %d\n", create_thread(thread_a));
  for (int i = 0; i < 100000; i++) {
    /* some long running unnecessary loop */
  }

  return 0;
}