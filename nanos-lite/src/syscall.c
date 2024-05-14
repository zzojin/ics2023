#include <common.h>
#include <stdint.h>
#include "syscall.h"
#include "am.h"


#define SYSCALL_CASE(func) case SYS_##func: c->GPRx = sys_##func(a); break;

static uintptr_t sys_exit() {
    halt(0);
    return 0;
}

static uintptr_t sys_yield() {
    Log("sys yield");
    yield();                        // yield 执行过程中会触发 ecall 指令，即系统调用内再次调用 ecall 指令
    return 0;
}

void do_syscall(Context *c) {
  uintptr_t a[4];
  a[0] = c->GPR1;               // 系统调用号, a7 寄存器
  a[1] = c->GPR2;
  a[2] = c->GPR3;
  a[3] = c->GPR4;

  switch (a[0]) {
      SYSCALL_CASE(exit);
      SYSCALL_CASE(yield);
      default: panic("Unhandled syscall ID = %d", a[0]);
  }
}
