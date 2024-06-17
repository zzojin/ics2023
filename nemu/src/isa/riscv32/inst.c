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
*
* 本代码要与 decode.h 结合起来看
* 包含了 pattern 匹配和操作数提取两部分功能
* decode_exec -> INSTPAT（操作码匹配，含有函数 pattern_decode）-> INSTPAT_MATCH（操作数匹配, decode_operand）
***************************************************************************************/

#include "common.h"
#include "debug.h"
#include "isa.h"
#include "local-include/reg.h"
#include "../../utils/ftrace.h"
#include "macro.h"
#include <cpu/cpu.h>
#include <cpu/ifetch.h>
#include <cpu/decode.h>
#include <stdint.h>

#define R(i) gpr(i)
#define Mr vaddr_read
#define Mw vaddr_write

void trace_inst(word_t pc, uint32_t inst);
void trace_exception(word_t NO, word_t pc);

enum {
  TYPE_R, TYPE_I, TYPE_U, TYPE_S, TYPE_J, TYPE_B,
  TYPE_N, // none
};

// 这里的 src1 需要解引用是因为在 decode_operand 传递参数时，取的是 src1 的地址，因此此处的 src1 确实是一个地址
#define src1R() do { *src1 = R(rs1); } while (0)
#define src2R() do { *src2 = R(rs2); } while (0)
// 立即数在不同类型的指令中的位置不同, SEXT 的作用是做符号扩展，符号扩展主要是照顾 BITS 从变量 i 中抽出的是一个无符号的数字，不能直接表示负数。这组数字不能直接赋值给一个 Int64_t，这样只会得到正数。只能手动构造出 64 位的有符号数。例如 immI 抽出的 12 位数字是 12 个 1，首位是 1，扩展成 64 位，那么自动将前 52 位全部变成 1，然后再转成 uint64_t
// 最终的赋值式子是 *imm = (uint64_t) __x.n
#define immI() do { *imm = SEXT(BITS(i, 31, 20), 12); } while(0)
// 与上一条命令刚好相反，这条命令的立即数有 20 位，并且最低的有效位是第 12 位（假设索引从 0 开始），刚好与 immI 的低 12 位互补。两者求和的话，可以表示完整的 32 位数字
#define immU() do { *imm = SEXT(BITS(i, 31, 12), 20) << 12; } while(0)
// 31~25 共 7 位做高位，11~7 共 5 位做低位，两者拼接成完整的立即数，并且高位的首位决定最终数字的正负号
#define immS() do { *imm = (SEXT(BITS(i, 31, 25), 7) << 5) | BITS(i, 11, 7); } while(0)

#define immJ() do { *imm = (SEXT(BITS(i, 31, 31), 1) << 20) | BITS(i, 19, 12) << 12 | BITS(i, 20, 20) << 11 | BITS(i, 30, 21) << 1;} while(0)

#define immB() do { *imm = (SEXT(BITS(i, 31, 31), 1) << 12) | BITS(i, 7, 7) << 11 | BITS(i, 30, 25) << 5 | BITS(i, 11, 8) << 1;} while(0)

static word_t* csr_value(word_t imm) {
    switch (imm) {
        case 0x300: return (word_t *)&cpu.mstatus;
        case 0x305: return &cpu.mtvec;
        case 0x341: return &cpu.mepc;
        case 0x342: return &cpu.mcause;
        case 0x180: return &cpu.satp;
        case 0x340: return &cpu.mscratch;
        default: panic("unknown csr register address %x", imm);
    }
}

static void decode_operand(Decode *s, int *rd, word_t *src1, word_t *src2, word_t *imm, int type) {
  uint32_t i = s->isa.inst.val;
  // BITS 通过位运算，抽取出二进制码中的某些位的数字，位的索引从 0 开始，最大是 31
  int rs1 = BITS(i, 19, 15);
  int rs2 = BITS(i, 24, 20);
  *rd     = BITS(i, 11, 7);
  switch (type) {
    case TYPE_I: src1R();          immI(); break;
    case TYPE_U:                   immU(); break;
    case TYPE_S: src1R(); src2R(); immS(); break;
    case TYPE_J:                   immJ(); break;
    case TYPE_B: src1R(); src2R(); immB(); break;
    case TYPE_R: src1R(); src2R();       ; break;
  }
}

static int decode_exec(Decode *s) {
  int rd = 0;
  word_t src1 = 0, src2 = 0, imm = 0;
  s->dnpc = s->snpc;

// INSTPAT_INST and INSTPAT_MATCH 在 INSTPAT 中被用到，INSTPAT 定义在 decode.h
// match 宏的作用是抽取操作数 
#define INSTPAT_INST(s) ((s)->isa.inst.val)
// 显示写出三个参数的名称，因此 __VA_ARGS__ 表示第四个及其后面的参数。
// 由于 INSTPAT_MATCH 的参数就是 INSTPAT 的参数，所以 __VA_ARGS__ 实际上就是指令执行语句
#define INSTPAT_MATCH(s, name, type, ... /* execute body */ ) { \
  decode_operand(s, &rd, &src1, &src2, &imm, concat(TYPE_, type)); \
  __VA_ARGS__ ; \
}

  // INSTPAT_START 创建一个标签地址，INSTAPAT_END 即是标签地址的位置。
  // 在匹配到某个 pattern 之后跳转到 END，有些类似于循环结构中的 break 的作用
  // INSTPAT 匹配 pattern，然后再通过 INSTPAT_INST INSTPAT_MATCH 提取操作数
  INSTPAT_START();
  INSTPAT("??????? ????? ????? ??? ????? 00101 11", auipc  , U, R(rd) = s->pc + imm);
  // load 
  INSTPAT("??????? ????? ????? ??? ????? 01101 11", lui    , U, R(rd) = imm);
  
  INSTPAT("??????? ????? ????? 100 ????? 00000 11", lbu    , I, R(rd) = Mr(src1 + imm, 1));
  INSTPAT("??????? ????? ????? 000 ????? 00000 11", lb     , I, R(rd) = (word_t)((int32_t)Mr(src1 + imm, 1) << 24 >> 24));
  INSTPAT("??????? ????? ????? 101 ????? 00000 11", lhu    , I, R(rd) = Mr(src1 + imm, 2));
  INSTPAT("??????? ????? ????? 001 ????? 00000 11", lh     , I, R(rd) = (word_t)((int32_t)Mr(src1 + imm, 2) << 16 >> 16));
  INSTPAT("??????? ????? ????? 010 ????? 00000 11", lw     , I, R(rd) = Mr(src1 + imm, 4));
  // Integer register-immediate 
  INSTPAT("??????? ????? ????? 000 ????? 00100 11", addi   , I, R(rd) = src1 + imm);
  INSTPAT("??????? ????? ????? 010 ????? 00100 11", slti   , I, R(rd) = (int32_t)src1 < (int32_t)imm);
  INSTPAT("??????? ????? ????? 011 ????? 00100 11", sltiu  , I, R(rd) = src1 < imm);
  INSTPAT("??????? ????? ????? 111 ????? 00100 11", andi   , I, R(rd) = src1 & imm);
  INSTPAT("??????? ????? ????? 110 ????? 00100 11", ori    , I, R(rd) = src1 | imm);
  INSTPAT("??????? ????? ????? 100 ????? 00100 11", xori   , I, R(rd) = src1 ^ imm);
  INSTPAT("0000000 ????? ????? 001 ????? 00100 11", slli   , I, R(rd) = src1 << imm);
  INSTPAT("0000000 ????? ????? 101 ????? 00100 11", srli   , I, R(rd) = src1 >> imm);
  INSTPAT("0100000 ????? ????? 101 ????? 00100 11", srai   , I, R(rd) = (int32_t)src1 >> (imm << 2 >> 2));

  // Integer register-register
  INSTPAT("0000000 ????? ????? 000 ????? 01100 11", add    , R, R(rd) = src1 + src2);
  INSTPAT("0100000 ????? ????? 000 ????? 01100 11", sub    , R, R(rd) = src1 - src2);
  INSTPAT("0000000 ????? ????? 010 ????? 01100 11", slt    , R, R(rd) = (int32_t)src1 < (int32_t)src2);
  INSTPAT("0000000 ????? ????? 011 ????? 01100 11", sltu   , R, R(rd) = src1 < src2);
  INSTPAT("0000000 ????? ????? 111 ????? 01100 11", and    , R, R(rd) = src1 & src2);
  INSTPAT("0000000 ????? ????? 110 ????? 01100 11", or     , R, R(rd) = src1 | src2);
  INSTPAT("0000000 ????? ????? 100 ????? 01100 11", xor    , R, R(rd) = src1 ^ src2);
  INSTPAT("0000000 ????? ????? 001 ????? 01100 11", sll    , R, R(rd) = src1 << (src2 & 0x1f));
  INSTPAT("0000000 ????? ????? 101 ????? 01100 11", srl    , R, R(rd) = src1 >> (src2 & 0x1f));
  INSTPAT("0100000 ????? ????? 101 ????? 01100 11", sra    , R, R(rd) = (int32_t)src1 >> (src2 & 0x1f));

  // Integer multiplication and division
  INSTPAT("0000001 ????? ????? 000 ????? 01100 11", mul    , R, R(rd) = src1 * src2);
  INSTPAT("0000001 ????? ????? 001 ????? 01100 11", mulh   , R, R(rd) = (int32_t)((SEXT((int64_t)src1, 32) * SEXT((int64_t)src2, 32)) >> 32));
  INSTPAT("0000001 ????? ????? 011 ????? 01100 11", mulhu  , R, R(rd) = (uint32_t)(((uint64_t)src1 * (uint64_t)src2) >> 32));
  // src2 会进行无符号扩展，提升至 64 位
  INSTPAT("0000001 ????? ????? 010 ????? 01100 11", mulhsu , R, R(rd) = (int32_t)(((int64_t)src1 * src2) >> 32));
  INSTPAT("0000001 ????? ????? 100 ????? 01100 11", div    , R, R(rd) = (int32_t)src1 / (int32_t)src2);
  INSTPAT("0000001 ????? ????? 101 ????? 01100 11", divu   , R, R(rd) = src1 / src2);
  INSTPAT("0000001 ????? ????? 110 ????? 01100 11", rem    , R, R(rd) = (int32_t)src1 % (int32_t)src2);
  INSTPAT("0000001 ????? ????? 111 ????? 01100 11", remu   , R, R(rd) = src1 % src2);

  // branch
  INSTPAT("??????? ????? ????? 000 ????? 11000 11", beq    , B, s->dnpc = src1 == src2 ? s->pc + imm : s->dnpc);
  INSTPAT("??????? ????? ????? 001 ????? 11000 11", bne    , B, s->dnpc = src1 == src2 ? s->dnpc : s->pc + imm);
  INSTPAT("??????? ????? ????? 100 ????? 11000 11", blt    , B, s->dnpc = (int32_t)src1 < (int32_t)src2 ? s->pc + imm : s->dnpc);
  INSTPAT("??????? ????? ????? 110 ????? 11000 11", bltu   , B, s->dnpc = src1 < src2 ? s->pc + imm : s->dnpc);
  INSTPAT("??????? ????? ????? 101 ????? 11000 11", bge    , B, s->dnpc = (int32_t)src1 >= (int32_t)src2 ? s->pc + imm : s->dnpc);
  INSTPAT("??????? ????? ????? 111 ????? 11000 11", bgeu   , B, s->dnpc = src1 >= src2 ? s->pc + imm : s->dnpc);

  // jump
  INSTPAT("??????? ????? ????? ??? ????? 11011 11", jal    , J, R(rd) = s->pc + 4, s->dnpc = s->pc + imm; IFDEF(CONFIG_FTRACE, process_jal(rd, s->pc, s->dnpc)));
  INSTPAT("??????? ????? ????? 000 ????? 11001 11", jalr   , I, R(rd) = s->pc + 4, s->dnpc = src1 + imm, s->dnpc = s->dnpc >> 1 << 1; IFDEF(CONFIG_FTRACE, process_jalr(rd, s->isa.inst.val, s->pc, s->dnpc, imm)));
  // store 
  INSTPAT("??????? ????? ????? 000 ????? 01000 11", sb     , S, Mw(src1 + imm, 1, src2));
  INSTPAT("??????? ????? ????? 001 ????? 01000 11", sh     , S, Mw(src1 + imm, 2, src2));
  INSTPAT("??????? ????? ????? 010 ????? 01000 11", sw     , S, Mw(src1 + imm, 4, src2));

  INSTPAT("0000000 00001 00000 000 00000 11100 11", ebreak , N, NEMUTRAP(s->pc, R(10))); // R(10) is $a0
  INSTPAT("??????? ????? ????? 001 ????? 11100 11", csrrw  , I, R(rd) = *csr_value(imm); *csr_value(imm) = src1);
  INSTPAT("??????? ????? ????? 010 ????? 11100 11", csrrs  , I, R(rd) = *csr_value(imm); *csr_value(imm) |= src1);                   
  INSTPAT("0000000 00000 00000 000 00000 11100 11", ecall  , N, IFDEF(CONFIG_ETRACE, trace_exception(0xb, s->pc)); s->dnpc = isa_raise_intr(0xb, s->pc)); // s->dnpc 是异常处理逻辑的地址: am_asm_trap，这个地址在 am 初始化时就固定到 mtvec 寄存器中了。   
  INSTPAT("0011000 00010 00000 000 00000 11100 11", mret   , N, s->dnpc = cpu.mepc; cpu.mstatus.bits.MIE = cpu.mstatus.bits.MPIE, cpu.mstatus.bits.MPIE = 1);       // TODO: mret 的实现 nosae 与 ics2022 有较大区别. 讲义中说 ecall 系统调用这类异常是需要 +4，但是像缺页故障类的异常，异常处理结束后，需要继续在发生异常的 pc 上执行。因此要不要+4 是需要做判断的。RISCV 的判断交由软件实现，x86 则由硬件实现. 判断逻辑应该由事件号决定，每种异常类型可能有一个或多个异常码. mcause 寄存器保存了异常码/中断码，这里我们一定运行在 machine 状态，故异常码写死=11
                                                                                           // pa3-1 Difftest 证明这里不能+4。看后续实验的进展
  INSTPAT("??????? ????? ????? ??? ????? ????? ??", inv    , N, INV(s->pc));
  INSTPAT_END();

  R(0) = 0; // reset $zero to 0

  return 0;
}

int isa_exec_once(Decode *s) {
  // riscv32 指令集长度固定，总是为 4 个字节，因此每执行一条指令，下一条指令的地址都在当前基础上加 4. 代码跳转的特殊情况例外。
  // 更新 snpc, +4
  s->isa.inst.val = inst_fetch(&s->snpc, 4);
  IFDEF(CONFIG_ITRACE, trace_inst(s->pc, s->isa.inst.val));
  return decode_exec(s);
}
