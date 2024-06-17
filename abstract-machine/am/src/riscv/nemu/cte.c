#include <am.h>
#include <riscv/riscv.h>
#include <klib.h>
#include <nemu.h>
#include <stdio.h>

#define Machine_Software_Interrupt (11)
#define User_Software_Interrupt (8)
#define IRQ_TIMER 0x80000007

static Context* (*user_handler)(Event, Context*) = NULL;
void __am_get_cur_as(Context *c);
void __am_switch(Context *c);

// trap.S 跳转过来. irq:interrupt request 
Context* __am_irq_handle(Context *c) {
  __am_get_cur_as(c);
  //printf("[__am_irq_handle]: c->pdir 内容地址修改前 页表项:%p\t上下文地址%p\t所在栈帧:%p\n", c->pdir, c, &c);
  //printf("ctx pdir %p\n", c->pdir);
  //int a = 0;
  if (user_handler) {
    Event ev = {0};
    switch (c->mcause) {
        case Machine_Software_Interrupt: 
        case User_Software_Interrupt:
            if (c->GPR1 == -1) { ev.event = EVENT_YIELD; break; }    // debug 加上 a = 1; // 可以观察到 yield 函数发起的 NO 就是 -1，并保存在 a7 寄存器.注意这个数不是异常码，更像是一种事件编号，系统调用编号等。
            else { ev.event = EVENT_SYSCALL; break; }
        case IRQ_TIMER:
            ev.event = EVENT_IRQ_TIMER;
            break;
        default:
            ev.event = EVENT_ERROR;
    }
    
    // mepc 修改，大多数情况是继续执行自陷指令的下一条指令，但是有几种情况例外
    switch (ev.event) {
        case EVENT_PAGEFAULT:
        case EVENT_ERROR:
        case EVENT_IRQ_TIMER:
            break;
        default:
            c->mepc += 4;
    }
    // 事件分发，交由事件处理函数 do_event(irq.c)，什么样的事件就执行对应的处理函数
    //printf("before schedule, c address = %p\n", c);
    c = user_handler(ev, c);
/*    printf("after schedule, c address = %p\n", c);
 *    printf("after schedule, sp = %p\n", c->gpr[2]);
 *    printf("after schedule, a0 GPRx = %p\n", c->GPRx);
 **/

    assert(c != NULL);
  }
/*if (a == 1)
 *        printf("after yield event,c pdir %p\n", c->pdir);  // 切换到内核线程之后，为何这个数值变成了内核的地址空间描述符指针，不应该继续保持 NULL 吗*/

  __am_switch(c);
  /*if (a == 1)
   *printf("after switch %p\n", c->pdir);
   *a = 0;*/
  // riscv 架构函数的的第一个入参和返回值都是放在 a0 寄存器，也就是说，此时如果发生了 yield，a0 寄存器被更换为另一个进程或线程的 context 地址。
  // 从而返回到 trap 时，实现了偷梁换柱，切换进程. 
  // 但这也有一个问题，也就是说在 user_handler 执行完之后，return 之前，虽然 c 变了，但是仍然处在前一个进程的用户栈上（PA4-2 阶段运行此段代码时不会 从用户栈切换内核栈），此时一旦 am_switch切换到新进程或新线程的地址空间，任何访存都是非法的。
  return c;
}

extern void __am_asm_trap(void);

bool cte_init(Context*(*handler)(Event, Context*)) {
  // initialize exception entry
  asm volatile("csrw mtvec, %0" : : "r"(__am_asm_trap));        // 把 trap.S __am_asm_trap 函数的地址写到 mtvec 寄存器

  // register event handler
  user_handler = handler;

  return true;
}

Context *kcontext(Area kstack, void (*entry)(void *), void *arg) {
    // stack.start = stack top 地址最小的那一头
  Context *ctx = (Context *)((uint8_t *)(kstack.end) - sizeof(Context));

  ctx->gpr[0] = 0;
  ctx->mepc = (uintptr_t)entry;
  //printf("==================kcontext=%x\n", ctx->mepc);
  ctx->mstatus = 0x1800 | MSTATUS_MPIE;         // 0x80 是MPIE，现在还不能添加，过不了 difftest 
  ctx->GPRx = (uintptr_t)arg;    // GPR2 和 GPRx 是一样的，都是 gpr[10]
  ctx->pdir = NULL;
  ctx->mscratch = 0;
  return ctx;
}

void yield() {
#ifdef __riscv_e
  asm volatile("li a5, -1; ecall");
#else
  asm volatile("li a7, -1; ecall");
#endif
}

bool ienabled() {
  return false;
}

void iset(bool enable) {
}
