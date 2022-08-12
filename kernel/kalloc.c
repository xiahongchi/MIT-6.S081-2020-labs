// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist[NCPU];
  struct spinlock cpulocks[NCPU];
} kmem;

char* kmemlknames[NCPU];
char namefield[NCPU*8];

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  for(int i = 0; i < NCPU; i++) {
    namefield[i*8] = 'k';
    namefield[i*8 + 1] = 'm';
    namefield[i*8 + 2] = 'e';
    namefield[i*8 + 3] = 'm';
    namefield[i*8 + 4] = '1' + i;
    namefield[i*8 + 5] = '\0';
    kmemlknames[i] = &namefield[i*8];
    initlock(&kmem.cpulocks[i], kmemlknames[i]);
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int currcpu;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  currcpu = cpuid();
  pop_off();

  acquire(&kmem.cpulocks[currcpu]);
  r->next = kmem.freelist[currcpu];
  kmem.freelist[currcpu] = r;
  release(&kmem.cpulocks[currcpu]);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int currcpu;

  push_off();
  currcpu = cpuid();
  pop_off();

  acquire(&kmem.cpulocks[currcpu]);
  r = kmem.freelist[currcpu];
  if(r)
    kmem.freelist[currcpu] = r->next;
  release(&kmem.cpulocks[currcpu]);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  else {
    for(int i = 1; i < NCPU; i++) {
      currcpu = (currcpu + 1) % NCPU;
      acquire(&kmem.cpulocks[currcpu]);
      r = kmem.freelist[currcpu];
      if(r) {
        kmem.freelist[currcpu] = r->next;
        release(&kmem.cpulocks[currcpu]);
        memset((char*)r, 5, PGSIZE); // fill with junk
        break;
      }
      release(&kmem.cpulocks[currcpu]);
    }
  }
  return (void*)r;
}
