#include <memory.h>

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

/* The brk() system call handler. */
int mm_brk(uintptr_t brk) {
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
