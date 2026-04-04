# Project 02 – Implementing `mmap()` in xv6

**Course:** CS333 – Operating Systems  
**Group Members:** 22125118, 23125055

---

## 1. Overview

This report describes our implementation of a simplified `mmap()` / `munmap()` system call pair in the xv6-RISC-V operating system. The goal was to enable multiple processes to share a single 4 KB physical memory page via a fixed virtual address, allowing direct inter-process communication through shared memory.

---

## 2. Design & Implementation

### 2.1 Global Shared Memory Structure

We defined a single global struct in `kernel/vm.c` to track the one shared page:

```c
struct {
  uint64 pa;           // Physical address of the shared page
  int refcount;        // Number of processes currently mapping it
  struct spinlock lock; // Protects concurrent access
  int allocated;       // Whether the page has been allocated
} shmem_page;

#define SHMEM_REGION 0x4000000  // Fixed VA at the 64 MB mark
```

The spinlock ensures that `refcount` and `allocated` are updated atomically, even on multicore systems.

### 2.2 Initialization – `init_shmem()`

Called from `kernel/main.c` at boot time (on CPU 0, after `userinit()`):

```c
void init_shmem(void) {
  initlock(&shmem_page.lock, "shmem");
  shmem_page.allocated = 0;
  shmem_page.refcount  = 0;
  shmem_page.pa        = 0;
}
```

### 2.3 `mmap()` – Mapping the Shared Page

Implemented in `kernel/vm.c`:

1. Acquires the spinlock.
2. If `allocated == 0`, calls `kalloc()` to get a fresh physical page and zeroes it with `memset`.
3. Calls `mappages()` to install a PTE at `SHMEM_REGION` in the calling process's page table with `PTE_R | PTE_W | PTE_U`.
4. Increments `refcount`.
5. Returns `SHMEM_REGION` on success, or `0` on any failure (also frees the physical page if it was just allocated and no other process holds a reference).

### 2.4 `munmap()` – Unmapping the Shared Page

Implemented in `kernel/vm.c`:

1. Validates that `va == SHMEM_REGION`; returns `-1` otherwise.
2. Uses `walk()` to find the PTE and confirms it is valid (`PTE_V` set).
3. Calls `uvmunmap()` with `do_free = 0` to clear the PTE without freeing physical memory.
4. Acquires the lock, decrements `refcount`.
5. If `refcount` reaches 0, calls `kfree()` on the physical page and resets `allocated`.

### 2.5 `fork()` – Sharing Across Parent and Child (`uvmcopy`)

We modified `uvmcopy()` (in `kernel/vm.c`) so that when a process forks, the shared page is **not** copied but is instead re-mapped into the child's page table at the same `SHMEM_REGION` address using `mappages()`. The `refcount` is incremented to track the new mapping.

```c
pte = walk(old, SHMEM_REGION, 0);
if (pte != 0 && (*pte & PTE_V)) {
    mappages(new, SHMEM_REGION, PGSIZE, shmem_page.pa, PTE_R|PTE_W|PTE_U);
    acquire(&shmem_page.lock);
    shmem_page.refcount++;
    release(&shmem_page.lock);
}
```

This ensures the parent and child both see each other's writes.

### 2.6 Process Cleanup (`uvmunmap`)

We modified `uvmunmap()` to detect when the address being freed is `SHMEM_REGION`. Instead of calling `kfree()` immediately (which would double-free a shared page), it decrements `refcount` and only calls `kfree()` when `refcount` reaches zero.

### 2.7 System Call Plumbing

| File | Change |
|------|--------|
| `kernel/syscall.h` | Added `SYS_mmap = 24`, `SYS_munmap = 25` |
| `kernel/syscall.c` | Declared `sys_mmap` / `sys_munmap` externally; registered in the syscall table |
| `kernel/sysproc.c` | Thin wrappers: `sys_mmap()` calls `mmap()`, `sys_munmap()` extracts address with `argaddr()` then calls `munmap()` |
| `kernel/defs.h` | Added prototypes: `init_shmem`, `mmap`, `munmap` |
| `user/user.h` | Added user-side declarations for `mmap()` and `munmap()` |
| `user/usys.pl` | Added `entry("mmap")` and `entry("munmap")` to generate syscall stubs |
| `Makefile` | Added `$U/_mmaptest` to `UPROGS` |

### 2.8 System Call Handlers in `kernel/sysproc.c`

The syscall dispatch layer in xv6 separates argument extraction from core logic. Both handlers live in `kernel/sysproc.c` and act as thin bridges to the actual implementations in `kernel/vm.c`.

**`sys_mmap()`**

```c
uint64
sys_mmap(void)
{
  return mmap();
}
```

`mmap()` takes no parameters, so `sys_mmap` simply forwards the call directly. All allocation and mapping logic is handled inside `mmap()` in `vm.c`. The return value is `SHMEM_REGION` (0x4000000) on success, or `0` on failure.

**`sys_munmap()`**

```c
uint64
sys_munmap(void)
{
  uint64 va;
  argaddr(0, &va);
  if (va != 0x4000000) {
    return -1;
  }
  return munmap(va);
}
```

`sys_munmap()` uses `argaddr(0, &va)` to safely read the first user-space argument (the virtual address to unmap) into kernel memory. It then applies an early guard — if `va` is not `SHMEM_REGION`, it returns `-1` immediately without entering the heavier `munmap()` logic. This double-checks the address at the syscall boundary, complementing the same check inside `munmap()` itself. If the address is valid, it delegates to `munmap(va)` in `vm.c` to perform the PTE removal and reference count decrement.

---

## 3. Testing

We compiled and ran `mmaptest` in the xv6 shell. The output matched the expected result exactly:

```
$ mmaptest
Testing mmap shared memory...
Mapped shared memory at 0x4000000
Parent wrote: 42
Child read: 42
Child wrote: 100
Child: munmap succeeded
Parent read after child: 100
Parent: munmap succeeded
```

This confirms:
- The shared page is correctly allocated and mapped at `SHMEM_REGION`.
- `fork()` propagates the mapping to the child without copying data.
- Writes by the child are immediately visible to the parent (true shared memory).
- Both processes can call `munmap()` independently; the physical page is freed only after both have unmapped.

---

## 4. Advanced Extension – Multiple Shared Memory Regions

The advanced implementation replaces the single `shmem_page` global with a **linked list** of `shmem_region` descriptors backed by a static pool, and extends the API to support dynamically allocated virtual addresses and named (id-based) regions.

### 4.1 New Data Structures

```c
#define SHMEM_BASE        0x4000000   // Base VA for shared memory window
#define MAX_SHMEM_REGIONS 16

struct shmem_region {
  uint64 va;                  // Virtual address (same in all sharing processes)
  uint64 pa;                  // Physical address
  int    refcount;            // # processes currently mapping this region
  int    id;                  // 0 = anonymous; >0 = named shared region
  uint   perm;                // PTE permission flags
  struct shmem_region *next;  // Next node in active list
};

// Static pool – entry is free when refcount==0 && pa==0
struct shmem_region shmem_pool[MAX_SHMEM_REGIONS];

struct {
  struct spinlock lock;        // Single lock protects list + pool
  struct shmem_region *head;   // Head of active-region linked list
} shmem_system;
```

Using a static pool avoids calling `kalloc()` for metadata, keeping allocation simple and bounded.

### 4.2 `init_shmem()` – Initialization

```c
void init_shmem(void) {
  initlock(&shmem_system.lock, "shmem_system");
  shmem_system.head = 0;
  memset(shmem_pool, 0, sizeof(shmem_pool));
}
```

### 4.3 New `mmap(int prot, int id)` API

The signature now takes:
- `prot` – a `PROT_READ | PROT_WRITE | PROT_EXEC` bitmask (values 1/2/4)
- `id` – `0` for a new anonymous region; `>0` to attach to an existing named region

```c
uint64 mmap(int prot, int id) {
  // Build PTE perm from prot ...
  acquire(&shmem_system.lock);

  if(id > 0) {
    struct shmem_region *r = shmem_find_by_id(id); // O(n) list scan
    if(r) {
      uint64 va = shmem_find_free_va(p->pagetable); // scan SHMEM_BASE window
      mappages(p->pagetable, va, PGSIZE, r->pa, r->perm);
      r->refcount++;
      release(&shmem_system.lock);
      return va;
    }
  }

  // New region: find free pool slot, free VA, kalloc physical page
  struct shmem_region *r = shmem_alloc_entry();
  uint64 va = shmem_find_free_va(p->pagetable);
  uint64 pa = (uint64)kalloc(); memset((void*)pa, 0, PGSIZE);
  mappages(p->pagetable, va, PGSIZE, pa, perm);

  // Prepend to active list
  r->va = va; r->pa = pa; r->refcount = 1; r->id = id; r->perm = perm;
  r->next = shmem_system.head; shmem_system.head = r;
  release(&shmem_system.lock);
  return va;
}
```

Key design points:
- VA is **dynamically chosen** by scanning `[SHMEM_BASE, SHMEM_BASE + 16*PGSIZE)` for an unmapped page in the calling process's page table.
- A named region (`id > 0`) is found in O(n) by list scan; any future caller attaching by the same `id` gets the same physical page at its own free VA.

### 4.4 `munmap(uint64 va)`

```c
uint64 munmap(uint64 va) {
  acquire(&shmem_system.lock);
  struct shmem_region *r = shmem_find_by_va(va); // list lookup
  // validate PTE, clear with uvmunmap(do_free=0)
  r->refcount--;
  if(r->refcount == 0) {
    // splice out of list, kfree PA, memset pool slot → free
  }
  release(&shmem_system.lock);
  return 0;
}
```

### 4.5 `shmem_proc_cleanup(pagetable_t)` – Safe Process Exit

A new function iterates the active list and removes any regions still mapped in a dying process's page table, preventing `freewalk` from panicking on residual leaf PTEs:

```c
void shmem_proc_cleanup(pagetable_t pagetable) {
  acquire(&shmem_system.lock);
  for each r in shmem_system.head:
    if PTE at r->va is valid in pagetable:
      uvmunmap(pagetable, r->va, 1, 0);
      r->refcount--;
      if(r->refcount == 0) { kfree PA; remove from list; reset pool slot; }
  release(&shmem_system.lock);
}
```

Called from `proc_freepagetable()` in `kernel/proc.c` before `uvmfree`.

### 4.6 `uvmcopy()` – Fork Propagation

Replaced the single fixed-address check with an iteration over the active list:

```c
acquire(&shmem_system.lock);
for(struct shmem_region *r = shmem_system.head; r; r = r->next) {
  pte_t *sp = walk(old, r->va, 0);
  if(sp && (*sp & PTE_V)) {
    mappages(new, r->va, PGSIZE, r->pa, r->perm);
    r->refcount++;
  }
}
release(&shmem_system.lock);
```

### 4.7 Modified Files

| File | Change |
|------|--------|
| `kernel/vm.c` | New `shmem_region` struct, static pool, `shmem_system`; rewrote `init_shmem`, `mmap`, `munmap`; added `shmem_proc_cleanup`; updated `uvmcopy` |
| `kernel/proc.c` | Added `shmem_proc_cleanup(pagetable)` call in `proc_freepagetable` |
| `kernel/defs.h` | Updated prototypes: `mmap(int,int)`, `munmap(uint64)`, `shmem_proc_cleanup(pagetable_t)` |
| `kernel/sysproc.c` | `sys_mmap` now extracts `prot` and `id` via `argint`; removed hardcoded address guard in `sys_munmap` |
| `user/user.h` | Updated: `uint64 mmap(int prot, int id)` |
| `user/mmaptest.c` | Updated call: `mmap(3, 42)` (read+write, named region id=42) |

---

## 5. Known Issues / Limitations

- `shmem_find_by_va` and `shmem_find_by_id` are O(n) scans; with `MAX_SHMEM_REGIONS = 16` this is negligible.
- Two unrelated processes cannot yet share by `id` across `fork` boundaries — they would need to agree on the `id` before forking or via a separate coordination mechanism.
- `init_shmem()` is called after `userinit()` in `main.c`. This is safe because no user process actually runs until the scheduler starts, but ideally it would be called earlier.

---

## 6. Conclusion

We successfully implemented a simplified `mmap()` / `munmap()` pair in xv6 supporting shared memory between parent and child processes via reference-counted physical pages. The base implementation uses a single global page; the advanced extension generalises this to up to 16 independent named or anonymous regions using a linked-list design with a static pool, dynamic virtual address allocation, and safe cleanup on process exit.


