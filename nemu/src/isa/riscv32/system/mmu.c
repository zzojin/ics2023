/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include "common.h"
#include "debug.h"
#include <isa.h>
#include <memory/vaddr.h>
#include <memory/paddr.h>

typedef uint32_t PTE;
#define PTE_V 0x01
#define PTE_R 0x02
#define PTE_W 0x04
#define PTE_X 0x08
#define PTE_U 0x10
#define PTE_A 0x40
#define PTE_D 0x80

#define PTE_PPN_MASK (0xFFFFFC00u)
#define PTE_PPN(x) (((x) & PTE_PPN_MASK) >> 10)
#define PGT1_ID(val) (val >> 22)
#define PGT2_ID(val) ((val & 0x3fffff) >> 12)
#define OFFSET(val) (val & 0xfff)

paddr_t isa_mmu_translate(vaddr_t vaddr, int len, int type) {
    paddr_t pte_1_addr = (cpu.satp << 12) + PGT1_ID(vaddr) * 4;   // 每个表项占 4 个字节，计算得到虚拟地址的一级页面表项地址
    PTE pte_1 = paddr_read(pte_1_addr, sizeof(PTE));
    Assert(pte_1 & PTE_V, "first class pte is not valid, vaddr=%#x", vaddr);

    paddr_t pte_2_addr = (PTE_PPN(pte_1) << 12) + PGT2_ID(vaddr) * 4; // 高 22 位是 PPN，physical page number, 物理页号. 但是在 4KB 页面的设置下，高两位被直接移出去了
    PTE pte_2 = paddr_read(pte_2_addr, sizeof(PTE));
    Assert(pte_2 & PTE_V, "second class pte is not valid, vaddr=%#x", vaddr);

    // 记录访问、写入标志。0 是取指，1 是读取，2 是
    paddr_write(pte_2_addr, 4, type == MEM_TYPE_WRITE ? pte_2 | PTE_A | PTE_D : pte_2 | PTE_A);

    paddr_t pa = PTE_PPN(pte_2) << 12 | OFFSET(vaddr);
    //Assert(pa == vaddr, "get physical address wrong, pa=%#x, va=%#x", pa, vaddr);
    return pa;
}
