#include <common.h>
#include <stdint.h>
#include "syscall.h"
#include "am.h"
#include "debug.h"

//#define CONFIG_STRACE
// 宏定义的内部不能出现其他预处理指令, 宏不能被其他预处理指令中断
/*#ifdef CONFIG_STRACE*/
/*#define TRACE_CALL(func, a, ret) strace(func, a, ret)*/
/*#else*/
/*#define TRACE_CALL(func, a, ret)*/
/*#endif*/

#define SYSCALL_CASE(func) case SYS_##func: c->GPRx = sys_##func(a); strace(#func, a, c->GPRx); break;

static void strace(char *s, uintptr_t *a, uintptr_t ret) {
#ifdef CONFIG_STRACE
    Log("[strace]%s: a0=%x a1=%x a2=%x a3=%x, ret=%d", s, a[0], a[1], a[2], a[3], ret);
#endif
}

static uintptr_t sys_exit(uintptr_t *a) {
    Log("[strace]exit");                    // exit 系统调用使用 halt 函数实现的时候，直接就出去了，走不到 TRACE_CALL 的逻辑，所以手动在这里添加一个日志
    halt(0);
    return 0;
}

static uintptr_t sys_yield(uintptr_t *a) {
    yield();                        // yield 执行过程中会触发 ecall 指令，即系统调用内再次调用 ecall 指令
    return 0;
}

static uintptr_t sys_write(uintptr_t *a) {
    int fd = a[1];
    char *buf = (char *)a[2];
    size_t cnt = a[3];
    size_t num = cnt;
    // 标准输出和标准错误
    if (fd == 1 || fd == 2) {
        while (cnt) {
            putch(*(buf++));
            cnt--;
        }
        return num - cnt;
    }
    // 输出到文件
    //return fs_write(fd, buf, cnt);
    return num - cnt;
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
      SYSCALL_CASE(write);
      default: panic("Unhandled syscall ID = %d", a[0]);
  }
}
