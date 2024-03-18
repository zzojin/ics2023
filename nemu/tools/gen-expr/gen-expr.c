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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
//#include "../../src/monitor/sdb/sdb.h"

// this should be enough
static int depth = 0;
static char buf[65536] = {};
static unsigned int buf_length = 0;
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";

static void gen(char c) {
    char char_buffer[2] = {c, '\0'};
    strcat(buf, char_buffer);
    buf_length++;
}

static void gen_num() {
    int num = rand() % 1000;
    char num_buffer[15];
    snprintf(num_buffer, 14, "%uu", num);
    strcat(buf, num_buffer);
    buf_length += strlen(num_buffer);
}

static char gen_rand_op(){
  buf_length++;
  switch (rand() % 4){
    case 0:
      gen('+');
      return '+';
    case 1:
      gen('-');
      return '-';
    case 2:
      gen('*');
      return '*';
    default:
      gen('/');
      return '/';
  }
}

static void gen_blank() {
    int num = rand() % 10;
    char c[5];
    switch (num) {
        case 1:
            c[0] = ' ';
            c[1] = '\0';
            buf_length++;
            break;
        case 2:
            c[0] = ' ';
            c[1] = ' ';
            c[2] = '\0';
            buf_length += 2;
            break;
        case 3:
            c[0] = ' ';
            c[1] = ' ';
            c[2] = ' ';
            c[3] = '\0';
            buf_length += 3;
            break;
        default:
            c[0] = '\0';
            break;
    }
    strcat(buf, c);
}

static void gen_rand_expr(bool *valid) {
    // 快结束时，插入最后一个不会继续递归的表达式
    if (!*valid || buf_length >= 65536 - 10000 || depth > 15) {
        gen('(');
        gen_blank();
        gen_num();
        gen(')');
        return;
    }

    char op = ' ';
    int start = buf_length;
    gen_blank();
    switch(rand() % 3) {
        case 0: gen_num(); break;
        case 1: gen('('); gen_rand_expr(valid); gen(')'); break;
        case 2: gen_rand_expr(valid); op = gen_rand_op(); gen_rand_expr(valid); break;
    }
    gen_blank();
    if (op != ' ') depth++;
    if (op == '/' && *valid) {
        int length = buf_length - start;
        char e[length + 1];
        // length 不包括末尾的空字符
        strncpy(e, buf + start, length);
        e[length] = '\0';
        //bool success = true;
        //unsigned int res = expr(e, &success);
        //if (!success || res == 0) 
        //    *valid = false;
    }
}
/* 
 * 创建表达式，sprintf将表达式嵌入到代码中
 * 编译代码，执行代码，得到表达式的计算结果
 * 打开一个管道，从管道末端读取输出，并 scanf 到变量 result 里
 */
int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i ++) {
    buf_length = 0;
    depth = 0;
    buf[0] = '\0';
    bool valid = true;
    gen_rand_expr(&valid);
    if (!valid) continue;
    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc /tmp/.code.c -o /tmp/.expr");
    if (ret != 0) continue;

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    uint32_t result;
    ret = fscanf(fp, "%u", &result);
    pclose(fp);

    printf("%u %s\n", result, buf);
  }
  return 0;
}
