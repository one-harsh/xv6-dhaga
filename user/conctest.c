#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void print0()
{
    for(int i = 0; i < 10000; i++)
        printf("0");
}

void print1()
{
    for(int i = 0; i < 10000; i++)
        printf("1");
}

void print2()
{
    for(int i = 0; i < 10000; i++)
        printf("2");
}

int
main(int argc, char *argv[])
{
    int tid0 = create_thread(&print0);
    if(tid0 < 0 ) {
        printf("tid = %d", tid0);
        exit(1);
    }
    
    int tid1 = create_thread(&print1);
    if(tid1 < 0 ) {
        printf("tid = %d", tid1);
        exit(1);
    }

    print2();

    exit(0);
}