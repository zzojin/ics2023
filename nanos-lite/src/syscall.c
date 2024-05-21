#include <common.h>
#include <stdint.h>
#include <fs.h>
#include <sys/time.h>
#include "syscall.h"
#include "am.h"
#include "amdev.h"
#include "debug.h"
#include "klib-macros.h"

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
    halt(a[1]);                             // syscall 除了第一个参数，还传递了第二个 status。所以第二个参数a[1]就是 status
    return 0;
}

static uintptr_t sys_yield(uintptr_t *a) {
    yield();                        // yield 执行过程中会触发 ecall 指令，即系统调用内再次调用 ecall 指令
    return 0;
}

static uintptr_t sys_open(uintptr_t *a) {
    const char *pathname = (const char *)a[1];
    int flags = a[2];
    int mode = a[3];
    return fs_open(pathname, flags, mode);
}

static uintptr_t sys_read(uintptr_t *a) {
    int fd = a[1];
    void *buf = (void *)a[2];
    size_t len = a[3];
    return fs_read(fd, buf, len);
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
    return fs_write(fd, buf, cnt);
}

static uintptr_t sys_lseek(uintptr_t *a) {
    return fs_lseek(a[1], a[2], a[3]);
}

static uintptr_t sys_close(uintptr_t *a) {
    return fs_close(a[1]);
}

static uintptr_t sys_brk(uintptr_t *a) {
    return 0;                   // always succeed
}

static uintptr_t sys_gettimeofday(uintptr_t *a) {
    struct timeval *tv = (struct timeval *)a[1];
    struct timezone *tz = (struct timezone *)a[2];
    uint64_t us = io_read(AM_TIMER_UPTIME).us;              // 微秒
    if (tv) {
        tv->tv_sec = us / (1000 * 1000);
        tv->tv_usec = us % (1000 * 1000);                   // 余数微秒
    }

    if (tz) {

    }
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
      SYSCALL_CASE(write);
      SYSCALL_CASE(brk);
      SYSCALL_CASE(read);
      SYSCALL_CASE(open);
      SYSCALL_CASE(lseek);
      SYSCALL_CASE(close);
      SYSCALL_CASE(gettimeofday);
      default: panic("Unhandled syscall ID = %d", a[0]);
  }
}
