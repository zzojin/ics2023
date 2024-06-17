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

#include <isa.h>

word_t isa_raise_intr(word_t NO, vaddr_t epc) {
  /* TODO: Trigger an interrupt/exception with ``NO''.
   * Then return the address of the interrupt/exception vector.
   */
  cpu.mcause = NO;
  cpu.mepc = epc;

  cpu.mstatus.bits.MPIE = cpu.mstatus.bits.MIE;
  cpu.mstatus.bits.MIE = 0;
  return cpu.mtvec;         // 早在 cte_init 的时候，就把 trap.S  中的 __am_asm_trap 的地址赋给了 mtvec 寄存器。也就是说 __am_asm_trap 是异常处理逻辑的起始地址.
                                // 当 ecall 指令从 asm trap 函数开始执行时，程序就真正的“自陷”。接下来，会根据 csr 寄存器的数据，回调 __am_irq_handle，它会根据 mcause 选择某一个异常处理函数
}

word_t isa_query_intr() {
    if ( cpu.mstatus.bits.MIE && cpu.INTR ) {
        cpu.INTR = false;
        return IRQ_TIMER;
    }
    return INTR_EMPTY;
}
