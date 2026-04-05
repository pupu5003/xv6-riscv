#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096

// Helper: print PASS/FAIL
static void check(int cond, char *msg) {
  if (cond) {
    printf("  [PASS] %s\n", msg);
  } else {
    printf("  [FAIL] %s\n", msg);
  }
}

// -------------------------------------------------------
// Test 1: Multiple mmap() calls return DIFFERENT addresses
// -------------------------------------------------------
void test_multiple_addresses(void) {
  printf("\n=== Test 1: Multiple mmap() return different addresses ===\n");

  uint64 a = mmap();
  uint64 b = mmap();
  uint64 c = mmap();

  check(a != 0, "mmap() #1 succeeds (addr != 0)");
  check(b != 0, "mmap() #2 succeeds (addr != 0)");
  check(c != 0, "mmap() #3 succeeds (addr != 0)");
  check(a != b, "region #1 and #2 have different addresses");
  check(b != c, "region #2 and #3 have different addresses");
  check(a != c, "region #1 and #3 have different addresses");

  printf("  addr1=0x%lx  addr2=0x%lx  addr3=0x%lx\n", a, b, c);

  munmap(a);
  munmap(b);
  munmap(c);
  printf("  All 3 regions unmapped.\n");
}

// -------------------------------------------------------
// Test 2: Regions are INDEPENDENT (data doesn't bleed)
// -------------------------------------------------------
void test_independent_data(void) {
  printf("\n=== Test 2: Regions are independent (no data bleed) ===\n");

  uint64 a = mmap();
  uint64 b = mmap();

  check(a != 0 && b != 0, "both mmap() calls succeed");

  int *pa = (int *)a;
  int *pb = (int *)b;

  *pa = 111;
  *pb = 222;

  check(*pa == 111, "region A holds value 111");
  check(*pb == 222, "region B holds value 222");
  check(*pa != *pb,  "region A and B have different values (independent)");

  munmap(a);
  munmap(b);
}

// -------------------------------------------------------
// Test 3: fork() shares ALL regions; child & parent see same data
// -------------------------------------------------------
void test_fork_shares_all(void) {
  printf("\n=== Test 3: fork() shares all active regions ===\n");

  uint64 a = mmap();
  uint64 b = mmap();

  check(a != 0 && b != 0, "both regions mapped before fork");

  int *pa = (int *)a;
  int *pb = (int *)b;
  *pa = 10;
  *pb = 20;

  int pid = fork();
  if (pid == 0) {
    // Child: verify it sees parent's writes
    check(*pa == 10, "child sees parent's write (10) in region A");
    check(*pb == 20, "child sees parent's write (20) in region B");

    // Child writes back different values
    *pa = 99;
    *pb = 88;

    munmap(a);
    munmap(b);
    exit(0);
  } else {
    wait(0);

    // Parent: verify it sees child's writes (shared physical page)
    check(*pa == 99, "parent sees child's write (99) in region A");
    check(*pb == 88, "parent sees child's write (88) in region B");

    munmap(a);
    munmap(b);
  }
}

// -------------------------------------------------------
// Test 4: munmap() one region does NOT affect others
// -------------------------------------------------------
void test_partial_unmap(void) {
  printf("\n=== Test 4: munmap() one region does not affect others ===\n");

  uint64 a = mmap();
  uint64 b = mmap();
  uint64 c = mmap();

  check(a != 0 && b != 0 && c != 0, "3 regions mapped");

  int *pa = (int *)a;
  int *pb = (int *)b;
  int *pc = (int *)c;

  *pa = 1; *pb = 2; *pc = 3;

  // Unmap middle region
  int r = munmap(b);
  check(r == 0, "munmap() on region B returns 0 (success)");

  // Other regions still accessible
  check(*pa == 1, "region A still holds value 1 after B unmapped");
  check(*pc == 3, "region C still holds value 3 after B unmapped");

  munmap(a);
  munmap(c);
}

// -------------------------------------------------------
// Test 5: munmap() same address twice returns -1 (error)
// -------------------------------------------------------
void test_double_unmap(void) {
  printf("\n=== Test 5: double munmap() returns error ===\n");

  uint64 a = mmap();
  check(a != 0, "mmap() succeeds");

  int r1 = munmap(a);
  check(r1 == 0, "first munmap() returns 0 (success)");

  int r2 = munmap(a);
  check(r2 != 0, "second munmap() on same addr returns error (-1)");
}

// -------------------------------------------------------
// Test 6: Process exit auto-cleans shared region (refcount)
// -------------------------------------------------------
void test_exit_cleanup(void) {
  printf("\n=== Test 6: child exit cleans up without double-free (refcount) ===\n");

  uint64 a = mmap();
  check(a != 0, "mmap() in parent succeeds");

  int *pa = (int *)a;
  *pa = 55;

  int pid = fork();
  if (pid == 0) {
    // Child exits WITHOUT calling munmap — kernel must decrement refcount
    check(*pa == 55, "child sees parent value 55");
    *pa = 77;
    // deliberately do NOT call munmap — rely on exit cleanup
    exit(0);
  } else {
    wait(0);
    // Parent must still be alive and data coherent
    check(*pa == 77, "parent sees child's write (77) after child exited cleanly");
    int r = munmap(a);
    check(r == 0, "parent munmap() after child exit succeeds (no double-free)");
  }
}

int main(void) {
  printf("===== Advanced mmap Multi-Region Test Suite =====\n");

  test_multiple_addresses();
  test_independent_data();
  test_fork_shares_all();
  test_partial_unmap();
  test_double_unmap();
  test_exit_cleanup();

  printf("\n===== All tests complete =====\n");
  exit(0);
}
