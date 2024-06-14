#include "am.h"
#include <memory.h>
#include <proc.h>

static void *pf = NULL;

void* new_page(size_t nr_page) {
    void *old_pf = pf;
    pf = (void *)((char *)pf + nr_page * PGSIZE);
    return old_pf;          // PA 手册要求返回地址段的首地址，所以返回 old_pf
}

#ifdef HAS_VME
static void* pg_alloc(int n) {
  assert(n % (4096) == 0);
  void *ret = new_page(n / (4096));
  memset(ret, 0, n);
  return ret;
}
#endif

void free_page(void *p) {
  panic("not implement yet");
}

/* The brk() system call handler. brk is end address of allocated space 
 * In map, pcb->max_brk is aligned to 4KB*/
extern PCB *current;
int mm_brk(uintptr_t brk) {
    if (brk <= current->max_brk)
        return 0;
    for (; current->max_brk < brk; current->max_brk += PGSIZE) {
        map(&current->as, (void *)current->max_brk, pg_alloc(PGSIZE), PTE_V | PTE_R | PTE_W | PTE_X);
    }
    return 0;
}

void init_mm() {
  pf = (void *)ROUNDUP(heap.start, PGSIZE);
  Log("free physical pages starting from %p", pf);

#ifdef HAS_VME
  Log("vme init");
  vme_init(pg_alloc, free_page);
#endif
}
