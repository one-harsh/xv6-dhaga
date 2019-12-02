#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


void *a = 0x0;

void
threadFunc()
{
    a = malloc(sizeof(int));
    
    if(a == 0x0)
    {
        printf("\nTest failed due to malloc error");
        exit(1);
    }
    printf("\nFrom child, modified a to %p", a);
}

//Stub for testing purposes
/*int create_thread(void (*f)())
{
    f();
    return 1;
}*/

int
main(int argc, char *argv[])
{   
  //int tid = create_thread((void *)&threadFunc);
  int tid = create_thread((uint64)&threadFunc);

  if(tid < 0)
  {
      printf("\nTest failed since thread creation failed");
      exit(1);
  }

  while(a == 0x0)
  {
      continue;
  }

  printf("\nMain thread sees the change in vaiable a: %p\n", a);
  exit(0);
}