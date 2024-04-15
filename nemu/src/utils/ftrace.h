#include <common.h>

#ifndef FTRACE_H
#define FTRACE_H
void process_jal(int rd, vaddr_t cur, vaddr_t dst);
void process_jalr(int rd, word_t src, vaddr_t cur, vaddr_t dst, word_t imm);
void print_func_stack();
#endif
