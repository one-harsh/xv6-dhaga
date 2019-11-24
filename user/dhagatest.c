#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
dhagatest()
{
    if(clone((void *)0,0)!=0)
    {
        printf("clone should return zero\n");
        exit(1);
    }

    printf("all tests passing\n");
}

int
main(void)
{
  dhagatest();
  exit(0);
}
