#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void print0()
{
    for(int i = 0; i < 10000; i++)
        printf(" ");
}

void print1()
{
    for(int i = 0; i < 10000; i++)
        printf("-");
}

void print2()
{
    for(int i = 0; i < 10000; i++)
        printf("_");
}

int
main(int argc, char *argv[])
{
    int tid0 = create_thread(&print0);
    if(tid0 < 0 ) {
        printf("tid = %d", tid0);
        exit(1);
    }
    
    printf("created 1st thread\n");

    int tid1 = create_thread(&print1);
    if(tid1 < 0 ) {
        printf("tid = %d", tid1);
        exit(1);
    }

    printf("created 2nd thread\n");
    print2();

    printf("GTFO\n");

    exit(0);
}