#include "kernel/types.h"
#include "kernel/getproc.h"
#include "user/user.h"

#define MAXPROC 64

char *
statename(int s)
{
    switch(s){
        case 0: return "unused";
        case 1: return "used";
        case 2: return "sleep";
        case 3: return "ready";
        case 4: return "run";
        case 5: return "zombie";
    }
    return "unknown";
}

int
main(void)
{
    struct procinfo p[MAXPROC];
    int n, i;

    // p is an array of procinfo structures to hold process information
    // p store the address of the first element of the array
    n = getprocs(p, MAXPROC); // return number of processes, call getprocs.c in kernel
    if(n < 0){
        printf("ps: getprocs failed\n");
        exit(1);
    }

    printf("PID\tSTATE\tSIZE\tNAME\n");
    for(i = 0; i < n; i++){
        printf("%d\t%s\t%d\t%s\n",
        p[i].pid,
        statename(p[i].state),
        (int)p[i].sz,
        p[i].name);
    }
    exit(0);
}
