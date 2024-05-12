#include <am.h>
#include <riscv/riscv.h>
#include <klib.h>

#define Machine_Software_Interrupt (11)
#define User_Software_Interrupt (8)

static Context* (*user_handler)(Event, Context*) = NULL;

// trap.S 跳转过来. irq:interrupt request
Context* __am_irq_handle(Context *c) {
  if (user_handler) {
    Event ev = {0};
    switch (c->mcause) {
        case Machine_Software_Interrupt: 
            if (c->GPR1 == -1) { ev.event = EVENT_YIELD; break; }     // 可以观察到 yield 函数发起的 NO 就是 -1，并保存在 a7 寄存器.注意这个数不是异常码，更像是一种事件编号，系统调用编号等。
            ev.event = EVENT_ERROR; break;
        default:
            ev.event = EVENT_ERROR;
    }
    // 事件分发，交由 trap 处理函数，什么样的事件就执行对应的处理函数
    c = user_handler(ev, c);
    // mepc 修改，大多数情况是继续执行自陷指令的下一条指令
    switch (ev.event) {
        case EVENT_PAGEFAULT:
        case EVENT_ERROR:
            break;
        default:
            c->mepc += 4;
    }
    assert(c != NULL);
  }

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
  return NULL;
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
