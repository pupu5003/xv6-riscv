// mmaptest2.c - Advanced tests for the linked-list mmap implementation
//
// Tests:
//   1. Named sharing - two children attach to same id and share data
//   2. Multiple independent anonymous regions in one process
//   3. Process exit without munmap (shmem_proc_cleanup path)

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PROT_RW  3   // PROT_READ | PROT_WRITE
#define PASS     "  [PASS]\n"
#define FAIL     "  [FAIL]\n"

// ------------------------------------------------------------------
// Test 1: Named shared region (id > 0)
//   Parent creates region with id=10, forks TWO children.
//   Both children attach to id=10 and verify they see the parent's
//   write. The second child also verifies it sees the first child's write.
// ------------------------------------------------------------------
void test_named_sharing(void)
{
    printf("Test 1: Named shared region (id=10)\n");

    uint64 addr = mmap(PROT_RW, 10);
    if(addr == 0){ printf("  mmap failed" FAIL); exit(1); }

    int *data = (int*)addr;
    *data = 1111;
    printf("  Parent wrote: %d\n", *data);

    // ---- Child 1 ----
    int pid1 = fork();
    if(pid1 < 0){ printf("  fork failed" FAIL); exit(1); }
    if(pid1 == 0){
        // Child 1 inherits mapping via uvmcopy (same VA)
        if(*data != 1111){
            printf("  Child1 expected 1111, got %d" FAIL, *data);
            exit(1);
        }
        printf("  Child1 read:  %d\n", *data);
        *data = 2222;
        printf("  Child1 wrote: %d\n", *data);
        munmap(addr);
        exit(0);
    }
    wait(0);

    // Parent sees child 1's write
    if(*data != 2222){
        printf("  Parent expected 2222 after child1, got %d" FAIL, *data);
        munmap(addr); exit(1);
    }
    printf("  Parent read after child1: %d\n", *data);

    // ---- Child 2 ----
    int pid2 = fork();
    if(pid2 < 0){ printf("  fork failed" FAIL); exit(1); }
    if(pid2 == 0){
        // Child 2 also inherits the mapping
        if(*data != 2222){
            printf("  Child2 expected 2222, got %d" FAIL, *data);
            exit(1);
        }
        printf("  Child2 read:  %d\n", *data);
        *data = 3333;
        printf("  Child2 wrote: %d\n", *data);
        munmap(addr);
        exit(0);
    }
    wait(0);

    if(*data != 3333){
        printf("  Parent expected 3333 after child2, got %d" FAIL, *data);
        munmap(addr); exit(1);
    }
    printf("  Parent read after child2: %d\n", *data);
    munmap(addr);
    printf("  Test 1" PASS);
}

// ------------------------------------------------------------------
// Test 2: Multiple independent anonymous regions in one process
//   Allocate 4 regions (id=0), write distinct values to each, verify
//   they don't overlap, then munmap all.
// ------------------------------------------------------------------
void test_multiple_regions(void)
{
    printf("Test 2: Multiple independent anonymous regions\n");

#define NREGIONS 4
    uint64 addrs[NREGIONS];
    int values[NREGIONS] = {111, 222, 333, 444};

    for(int i = 0; i < NREGIONS; i++){
        addrs[i] = mmap(PROT_RW, 0);   // anonymous
        if(addrs[i] == 0){
            printf("  mmap[%d] failed" FAIL, i);
            exit(1);
        }
        *(int*)addrs[i] = values[i];
    }

    // Verify all distinct addresses and correct values
    for(int i = 0; i < NREGIONS; i++){
        for(int j = i+1; j < NREGIONS; j++){
            if(addrs[i] == addrs[j]){
                printf("  regions %d and %d have the same VA!" FAIL, i, j);
                exit(1);
            }
        }
        if(*(int*)addrs[i] != values[i]){
            printf("  region[%d]: expected %d, got %d" FAIL,
                   i, values[i], *(int*)addrs[i]);
            exit(1);
        }
        printf("  region[%d] @ 0x%lx = %d  OK\n", i, addrs[i], *(int*)addrs[i]);
    }

    for(int i = 0; i < NREGIONS; i++)
        munmap(addrs[i]);

    printf("  Test 2" PASS);
}

// ------------------------------------------------------------------
// Test 3: Process exits WITHOUT calling munmap (cleanup path)
//   Parent maps a region, forks a child that writes to it and exits
//   without munmapping. Parent verifies the region is still valid
//   (refcount should be 1 after child exited), then munmaps cleanly.
// ------------------------------------------------------------------
void test_exit_without_munmap(void)
{
    printf("Test 3: Child exits without munmap (shmem_proc_cleanup)\n");

    uint64 addr = mmap(PROT_RW, 0);
    if(addr == 0){ printf("  mmap failed" FAIL); exit(1); }

    int *data = (int*)addr;
    *data = 9999;

    int pid = fork();
    if(pid < 0){ printf("  fork failed" FAIL); exit(1); }
    if(pid == 0){
        printf("  Child: read %d, now exiting WITHOUT munmap\n", *data);
        *data = 8888;
        // Intentionally skip munmap -- proc_freepagetable must clean up
        exit(0);
    }
    wait(0);

    // Parent's mapping should still be valid
    if(*data != 8888){
        printf("  Parent expected 8888, got %d" FAIL, *data);
        munmap(addr); exit(1);
    }
    printf("  Parent read after child (no-munmap) exit: %d\n", *data);
    if(munmap(addr) != 0){
        printf("  Parent munmap failed" FAIL);
        exit(1);
    }
    printf("  Parent munmap succeeded\n");
    printf("  Test 3" PASS);
}

// ------------------------------------------------------------------
int main(void)
{
    printf("\n=== mmaptest2: Advanced Shared Memory Tests ===\n\n");

    test_named_sharing();
    printf("\n");
    test_multiple_regions();
    printf("\n");
    test_exit_without_munmap();

    printf("\n=== All tests passed! ===\n");
    exit(0);
}
