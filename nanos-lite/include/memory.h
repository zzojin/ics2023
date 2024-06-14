#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <common.h>

#ifndef PGSIZE
#define PGSIZE 4096
#endif

#ifndef USER_STACK_PG_NUM
#define USER_STACK_PG_NUM 8
#endif
#ifndef PTE_V
#define PTE_V 0x01
#endif
#ifndef PTE_R
#define PTE_R 0x02
#endif
#ifndef PTE_W
#define PTE_W 0x04
#endif
#ifndef PTE_X
#define PTE_X 0x08
#endif
#ifndef PTE_U
#define PTE_U 0x10
#endif
#ifndef PTE_A
#define PTE_A 0x40
#endif
#ifndef PTE_D
#define PTE_D 0x80
#endif

#define PG_ALIGN __attribute((aligned(PGSIZE)))

void* new_page(size_t);
int mm_brk(uintptr_t brk);
#endif
