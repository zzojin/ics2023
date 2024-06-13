#include "riscv/riscv.h"
#include <am.h>
#include <nemu.h>
#include <klib.h>
#include <stdint.h>
#include <stdio.h>

static AddrSpace kas = {};
static void* (*pgalloc_usr)(int) = NULL;
static void (*pgfree_usr)(void*) = NULL;
static int vme_enable = 0;

static Area segments[] = {      // Kernel memory mappings
  NEMU_PADDR_SPACE
};

#define USER_SPACE RANGE(0x40000000, 0x80000000)

static inline void set_satp(void *pdir) {
  uintptr_t mode = 1ul << (__riscv_xlen - 1);
  asm volatile("csrw satp, %0" : : "r"(mode | ((uintptr_t)pdir >> 12)));
}

static inline uintptr_t get_satp() {
  uintptr_t satp;
  asm volatile("csrr %0, satp" : "=r"(satp));
  return satp << 12;
}

bool vme_init(void* (*pgalloc_f)(int), void (*pgfree_f)(void*)) {
  pgalloc_usr = pgalloc_f;
  pgfree_usr = pgfree_f;

  kas.ptr = pgalloc_f(PGSIZE);
  // 创建内核地址空间
  int i;
  for (i = 0; i < LENGTH(segments); i ++) {
    void *va = segments[i].start;
    for (; va < segments[i].end; va += PGSIZE) {
      map(&kas, va, va, PTE_R | PTE_W | PTE_X | PTE_V);
    }
  }
  set_satp(kas.ptr);
  printf("===========kernel as pdir %p==========\n", kas.ptr);
  vme_enable = 1;

  return true;
}

void protect(AddrSpace *as) {
  // 创建用户进程地址空间，并用内核地址空间为其初始化。所以内核的地址，用户程序可以访问到的。后续会往用户地址段继续设置地址映射。
  PTE *updir = (PTE*)(pgalloc_usr(PGSIZE));
  as->ptr = updir;
  as->area = USER_SPACE;
  as->pgsize = PGSIZE;
  // map kernel space
  memcpy(updir, kas.ptr, PGSIZE);
}

void unprotect(AddrSpace *as) {
}

void __am_get_cur_as(Context *c) {
  c->pdir = (vme_enable ? (void *)get_satp() : NULL);
}

void __am_switch(Context *c) {
  if (vme_enable && c->pdir != NULL) {
    set_satp(c->pdir);
  }
}

#define PTE_PPN_MASK (0xFFFFFC00u)
#define PTE_PPN(x) (((x) & PTE_PPN_MASK) >> 10)
#define PGT1_ID(val) (val >> 22)
#define PGT2_ID(val) ((val & 0x3fffff) >> 12)

void map(AddrSpace *as, void *va, void *pa, int prot) {
    va = (void *)((int)va & ~0xfff);
    pa = (void *)((int)pa & ~0xfff);
    PTE *pte_1 = as->ptr + PGT1_ID((uintptr_t)va) * 4;          // 与 4 做乘法，va 需要从 void * 转成 uint 或 int
    if (!(*pte_1 & PTE_V)) {
        void *allocated_page = pgalloc_usr(PGSIZE);
        // 构造 PTE
        *pte_1 = ((uintptr_t)allocated_page >> 2) | prot;  // PTE_V -> prot 会出错, 因为用户进程和内核创建虚拟地址空间时，给map 的传参不规范，现已更改;
    }
    PTE *pte_2 = (PTE *)((PTE_PPN(*pte_1) << 12) + PGT2_ID((uintptr_t)va) * 4);
    // 构造PTE，pa 的低 12 位在开始就已清零，现在创建 22 位的 PPN，往右移动 2 位。然后构造低 10 位的控制位
    //*pte_2 = ((uintptr_t)pa >> 2) | PTE_V | PTE_R | PTE_W | PTE_X | (prot ? PTE_U : 0);
    *pte_2 = ((uintptr_t)pa >> 2) | prot;
}

Context *ucontext(AddrSpace *as, Area kstack, void *entry) {
  Context *c = (Context *)kstack.end - 1;
  c->mepc = (uintptr_t)entry - 4;
  c->mstatus = 0xC0000 | 0x80;          // difftest  需要，创建用户进程时，设置成 U 模式,  且MXR=1 SUM=1
  c->pdir = as->ptr;
  printf("=============user context pdir=%p=============\n", c->pdir);
  //printf("entry=%x\n", c->mepc);
  return c;
}
