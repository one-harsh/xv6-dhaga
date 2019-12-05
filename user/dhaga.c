#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void child()
{
    printf("baby!\n");
}

int
main(int argc, char *argv[])
{
    printf("\n\ndhaga phat gaya\n\n");

    // int tid0 = create_thread(&child);
    // if(tid0 < 0 ) {
    //     printf("tid = %d", tid0);
    //     exit(1);
    // }
    
    printf("waiting\n");
    for(int i = 0; i < 10000000; i++);

    exit(0);
}