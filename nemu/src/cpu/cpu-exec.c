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

#include <cpu/cpu.h>
#include <cpu/decode.h>
#include <cpu/difftest.h>
#include <locale.h>
#include <stdio.h>
#include "../monitor/sdb/sdb.h"
#include "utils.h"
#include "../utils/ftrace.h"

/* The assembly code of instructions executed is only output to the screen
 * when the number of instructions executed is less than this value.
 * This is useful when you use the `si' command.
 * You can modify this value as you want.
 */
#define MAX_INST_TO_PRINT 10
#ifdef CONFIG_ITRACE_COND
#define LOG_CAP 16
  char iringbuf[LOG_CAP][LOG_LENGTH]; 
  size_t ringbuf_index = 0;
#endif

CPU_state cpu = {};
uint64_t g_nr_guest_inst = 0;
static uint64_t g_timer = 0; // unit: us
static bool g_print_step = false;

void device_update();

#ifdef CONFIG_ITRACE_COND
static void print_iringbuf() {
    printf("====== The nearest %d instructions ======\n", LOG_CAP);
    size_t end = ringbuf_index <= LOG_CAP ? ringbuf_index : LOG_CAP;
    for (int i = 0; i < end; i++) {
        if (i == (ringbuf_index - 1) % LOG_CAP) {
            printf(ANSI_FMT("%s\n", ANSI_FG_RED), iringbuf[i]);
        } else 
            printf("%s\n", iringbuf[i]);
    }
}
#endif

static void trace_and_difftest(Decode *_this, vaddr_t dnpc) {
// ITRACE_COND 就是由 CONFIG_ITRACE_COND 推断而来。这个宏的定义在 nemu/Makefile 中。因为 sdb.h common.h stdbool .h 最终 宏定义中的 true 和 false 都能正常被解释成 0 1
#ifdef CONFIG_ITRACE_COND
  if (ITRACE_COND) { log_write("%s\n", _this->logbuf); }
  strncpy(iringbuf[ringbuf_index % LOG_CAP], _this->logbuf, LOG_LENGTH);
  ringbuf_index++;
#endif
  // 打印指令地址、指令二进制码、反汇编结果
  if (g_print_step) { IFDEF(CONFIG_ITRACE, puts(_this->logbuf)); }
  IFDEF(CONFIG_DIFFTEST, difftest_step(_this->pc, dnpc));
#ifdef CONFIG_WATCHPOINT
  bool changed = check_wp();
  if (changed) {
      Log("hit watchpoint, pc is %x", cpu.pc);
      if (nemu_state.state == NEMU_RUNNING)
        nemu_state.state = NEMU_STOP;
  }
#endif
}

static void exec_once(Decode *s, vaddr_t pc) {
  s->pc = pc;
  s->snpc = pc;
  // 匹配并执行指令，更新 snpc 和 dnpc
  isa_exec_once(s);
  cpu.pc = s->dnpc;
  
  
  // 如果定义了追踪，还会记录每次执行的指令的二进制以及其反汇编代码
#ifdef CONFIG_ITRACE
  char *p = s->logbuf;
 
  // 第一步先获取地址，加一个英文冒号，将其写入到 p
  p += snprintf(p, sizeof(s->logbuf), FMT_WORD ":", s->pc);
 
  // 计算指令长度, 对于 riscv32 而言，ilen 通常都是 4
  int ilen = s->snpc - s->pc;
  int i;
 
  // s->isa.inst is an union,  val 是 uint32_t
  uint8_t *inst = (uint8_t *)&s->isa.inst.val;
  // 每次写入 1 个 uint8 类型的变量，即 1 个字节，重复 4 次，刚好是一个 riscv32 指令的长度。用 2 位 16 进制数表示一个 uint8 类型的变量
  for (i = ilen - 1; i >= 0; i --) {
    // 一个空格，两个 16 进制字符,位宽不够时用 0 填充
    p += snprintf(p, 4, " %02x", inst[i]);
  }
  int ilen_max = MUXDEF(CONFIG_ISA_x86, 8, 4);
  int space_len = ilen_max - ilen;
  if (space_len < 0) space_len = 0;
  // 添加空格的数量，保持格式对齐。x86 架构的指令长度是变长的，每个字节需要占用三个字符位置，包括十六进制 2 位和一个空格。此外，反汇编代码和指令二进制之间需要一个空格，所以有额外的 + 1
  space_len = space_len * 3 + 1;
  memset(p, ' ', space_len);
  p += space_len;
#ifndef CONFIG_ISA_loongarch32r
  void disassemble(char *str, int size, uint64_t pc, uint8_t *code, int nbyte);
  disassemble(p, s->logbuf + sizeof(s->logbuf) - p,
      MUXDEF(CONFIG_ISA_x86, s->snpc, s->pc), (uint8_t *)&s->isa.inst.val, ilen);
//printf("p:%s %ld\n", p, p - s->logbuf);
#else
  p[0] = '\0'; // the upstream llvm does not support loongarch32r
#endif
#endif
}

static void execute(uint64_t n) {
  Decode s;
  for (;n > 0; n --) {
    // 执行指令，同时更新 pc snpc dnpc
    exec_once(&s, cpu.pc);
    g_nr_guest_inst ++;
    trace_and_difftest(&s, cpu.pc);
    if (nemu_state.state != NEMU_RUNNING) break;
    IFDEF(CONFIG_DEVICE, device_update());
  }
}

static void statistic() {
  IFNDEF(CONFIG_TARGET_AM, setlocale(LC_NUMERIC, ""));
#define NUMBERIC_FMT MUXDEF(CONFIG_TARGET_AM, "%", "%'") PRIu64
  Log("host time spent = " NUMBERIC_FMT " us", g_timer);
  Log("total guest instructions = " NUMBERIC_FMT, g_nr_guest_inst);
  if (g_timer > 0) Log("simulation frequency = " NUMBERIC_FMT " inst/s", g_nr_guest_inst * 1000000 / g_timer);
  else Log("Finish running in less than 1 us and can not calculate the simulation frequency");
}

void assert_fail_msg() {
  isa_reg_display();
  statistic();
}

/* Simulate how the CPU works. */
void cpu_exec(uint64_t n) {
  g_print_step = (n < MAX_INST_TO_PRINT);
  switch (nemu_state.state) {
    case NEMU_END: case NEMU_ABORT:
      printf("Program execution has ended. To restart the program, exit NEMU and run again.\n");
      return;
    default: nemu_state.state = NEMU_RUNNING;
  }

  uint64_t timer_start = get_time();

  execute(n);

  uint64_t timer_end = get_time();
  g_timer += timer_end - timer_start;

  switch (nemu_state.state) {
    case NEMU_RUNNING: nemu_state.state = NEMU_STOP; break;

    case NEMU_END: case NEMU_ABORT:
      if (nemu_state.state == NEMU_ABORT || nemu_state.halt_ret != 0) {
          #ifdef CONFIG_ITRACE
              print_iringbuf();
          #endif
          #ifdef CONFIG_FTRACE
              print_func_stack();
          #endif
      }
      Log("nemu: %s at pc = " FMT_WORD,
          (nemu_state.state == NEMU_ABORT ? ANSI_FMT("ABORT", ANSI_FG_RED) :
           (nemu_state.halt_ret == 0 ? ANSI_FMT("HIT GOOD TRAP", ANSI_FG_GREEN) :
            ANSI_FMT("HIT BAD TRAP", ANSI_FG_RED))),
          nemu_state.halt_pc);

      // fall through
    case NEMU_QUIT: statistic();
  }
}
