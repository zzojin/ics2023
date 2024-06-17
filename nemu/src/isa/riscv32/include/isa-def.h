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

#ifndef __ISA_RISCV_H__
#define __ISA_RISCV_H__

#include <common.h>
typedef union {
    struct{
      uint32_t UIE    : 1;
      uint32_t SIE    : 1;
      uint32_t WPRI_0 : 1;
      uint32_t MIE    : 1;
      uint32_t UPIE   : 1;
      uint32_t SPIE   : 1;
      uint32_t WPRI   : 1;
      uint32_t MPIE   : 1;
      uint32_t SPP    : 1;
      uint32_t WPRI_1_2 : 2;
      uint32_t MPP    : 2;
      uint32_t FS     : 2;
      uint32_t XS     : 2;
      uint32_t MPRV   : 1;
      uint32_t SUM    : 1;
      uint32_t MXR    : 1;
      uint32_t TVM    : 1;
      uint32_t TW     : 1;
      uint32_t TSR    : 1;
      uint32_t WPRI_3_10 : 8;
      uint32_t SD     : 1;
    } bits;
    word_t value;
  } mstatus_register_t;

typedef struct {
  // 通用寄存器 general-purpose register(gpr)
  word_t gpr[MUXDEF(CONFIG_RVE, 16, 32)];
  vaddr_t pc;
  word_t mcause;
  vaddr_t mepc;
  mstatus_register_t mstatus;
  word_t mtvec;
  word_t satp;
  word_t mscratch;
  bool INTR;
} MUXDEF(CONFIG_RV64, riscv64_CPU_state, riscv32_CPU_state);

// decode
typedef struct {
  union {
    uint32_t val;
  } inst;
} MUXDEF(CONFIG_RV64, riscv64_ISADecodeInfo, riscv32_ISADecodeInfo);

//#define isa_mmu_check(vaddr, len, type) (MMU_DIRECT)
#define isa_mmu_check(vaddr, len, type) (BITS(cpu.satp, 31, 31) ? MMU_TRANSLATE : MMU_DIRECT)

#endif
