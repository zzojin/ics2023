#define concat_temp(x, y) x ## y
#define concat(x, y) concat_temp(x, y)
#define MAP(c, f) c(f)

#if __riscv_xlen == 32
#define LOAD  lw
#define STORE sw
#define XLEN  4
#else
#define LOAD  ld
#define STORE sd
#define XLEN  8
#endif

#define REGS_LO16(f) \
      f( 1)       f( 3) f( 4) f( 5) f( 6) f( 7) f( 8) f( 9) \
f(10) f(11) f(12) f(13) f(14) f(15)
#ifndef __riscv_e
#define REGS_HI16(f) \
                                    f(16) f(17) f(18) f(19) \
f(20) f(21) f(22) f(23) f(24) f(25) f(26) f(27) f(28) f(29) \
f(30) f(31)
#define NR_REGS 32
#else
#define REGS_HI16(f)
#define NR_REGS 16
#endif

#define REGS(f) REGS_LO16(f) REGS_HI16(f)

#define PUSH(n) STORE concat(x, n), (n * XLEN)(sp);
#define POP(n)  LOAD  concat(x, n), (n * XLEN)(sp);

#define CONTEXT_SIZE  ((NR_REGS + 3 + 1) * XLEN)
#define OFFSET_SP     ( 2 * XLEN)
#define OFFSET_CAUSE  ((NR_REGS + 0) * XLEN)
#define OFFSET_STATUS ((NR_REGS + 1) * XLEN)
#define OFFSET_EPC    ((NR_REGS + 2) * XLEN)

.align 3
.globl __am_asm_trap
__am_asm_trap:
  # 开辟新的栈空间，容纳待保存的 32 个通用寄存器（不包括 sp 寄存器和 x0 寄存器），以及 3 个 csr 寄存器, 和一个虚拟空间地址映射的页目录地址pdir
  addi sp, sp, -CONTEXT_SIZE
  # 通用寄存器压栈。这份汇编代码会先用 C 语言的预处理器处理宏，然后再进行编译工作，所以能够使用 C 语言的宏定义方式
  MAP(REGS, PUSH)
  
  # 控制寄存器压栈。将 csr 拷贝到通用寄存器，不能直接操作 csr 压栈
  csrr t0, mcause
  csrr t1, mstatus
  csrr t2, mepc
  
  # 压入控制与状态寄存器
  STORE t0, OFFSET_CAUSE(sp)
  STORE t1, OFFSET_STATUS(sp)
  STORE t2, OFFSET_EPC(sp)

  # set mstatus.MPRV to pass difftest
  li a0, (1 << 17)
  or t1, t1, a0
  csrw mstatus, t1
  
  # 在调用异常处理函数之前，把 sp 传递给 a0 寄存器。a0 将作为异常处理函数的参数，可以基于 a0 访问栈内保存的各种寄存器值和其他重要状态 
  # 很显然，这个 sp 的数值保存到 a0 中是给 am_irq_handle 做入参，刚好是一个 Context 变量的地址
  mv a0, sp
  # 已经保存了异常处理程序执行之前所有的上下文，现在跳转到位于 AM 的异常处理程序
  jal __am_irq_handle

  mv sp, a0

  # 异常处理结束，弹栈恢复异常出现之前的上下文
  LOAD t1, OFFSET_STATUS(sp)
  LOAD t2, OFFSET_EPC(sp)
  csrw mstatus, t1
  csrw mepc, t2

  MAP(REGS, POP)

  addi sp, sp, CONTEXT_SIZE
  mret
