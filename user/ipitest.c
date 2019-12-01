#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
ipitest()
{
    if(ipi() != 0)
    {
        printf("ipi should return zero\n");
        exit(1);
    }
}

int
main(void)
{
  ipitest();
  exit(0);
}