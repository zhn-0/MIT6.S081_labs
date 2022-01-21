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
  uint64 cnt[PHYSTOP/PGSIZE];
}ref;

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&ref.lock, "ref");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    ref.cnt[(uint64)p/PGSIZE] = 1;
    kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&ref.lock);
  if(--ref.cnt[(uint64)pa/PGSIZE]==0){
    release(&ref.lock);
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }else{
    release(&ref.lock);
    if(ref.cnt[(uint64)pa/PGSIZE]<0)
      panic("refcnt");
  }  
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r){
    kmem.freelist = r->next;
    acquire(&ref.lock);
    ref.cnt[(uint64)r/PGSIZE] = 1;
    release(&ref.lock);
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

uint64
refcnt(uint64 pa)
{
  acquire(&ref.lock);
  uint64 ret = ref.cnt[pa/PGSIZE];
  release(&ref.lock);
  return ret;
}

uint64
addrefcnt(uint64 pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    return -1;
  acquire(&ref.lock);
  ref.cnt[pa/PGSIZE]++;
  release(&ref.lock);
  return 0;
}

uint64
iscowpg(pagetable_t pagetable, uint64 va)
{
  if(va >= MAXVA)
    return -1;
  pte_t *pte = walk(pagetable, va, 0);
  if((pte == 0) || (*pte & PTE_U) == 0 || (*pte & PTE_V) == 0)
    return -1;
  return (*pte & PTE_RSW1) == 0 ? -1 : 0;
}

void*
cowalloc(pagetable_t pagetable, uint64 va)
{
  if(va%PGSIZE != 0)
    return 0;
  pte_t *pte = walk(pagetable, va, 0);
  uint64 pa = PTE2PA(*pte);
  if(pa == 0)
    return 0;
  if(refcnt(pa) == 1){
    *pte = (*pte | PTE_W) & ~PTE_RSW1;
    return (void*)pa;
  } else{
    char *mem;
    if((mem = kalloc()) == 0)
      return 0;
    memmove(mem, (void*)pa, PGSIZE);
    *pte &= ~PTE_V;
    if(mappages(pagetable, va, PGSIZE, (uint64)mem, (PTE_FLAGS(*pte) | PTE_W) & ~PTE_RSW1) != 0){
      kfree(mem);
      *pte |= PTE_V;
      return 0;
    }
    kfree((void*)PGROUNDDOWN(pa));
    return mem;
  }
}