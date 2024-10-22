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
/* sdb.h 不在 include 目录下，它的引入得是 ""，而不能用 <> */
#include <inttypes.h>
#include <isa.h>
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"
#include <stdio.h>
#include <string.h>
#include <utils.h>
#include <memory/vaddr.h>
#include <memory/paddr.h>

static int is_batch_mode = false;
static int qflag = 0;
void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
      // add_history 也是 realdline 库的函数，为 readline 函数读取方向键提供历史记录
      add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}


static int cmd_q(char *args) {
  set_nemu_state(NEMU_QUIT, 0, 0);
  return -1;
}

static int cmd_help(char *args);
static int cmd_si(char *args);
static int cmd_info(char *args);
static int cmd_x(char *args);
static int cmd_p(char *args);
static int cmd_w(char *args);
static int cmd_d(char *args);
static int cmd_save(char *args);
static int cmd_load(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },

  /* TODO: Add more commands */
  { "si", "Step N instruction", cmd_si },
  { "info", "print register or watchpoint", cmd_info },
  { "x", "print memory", cmd_x },
  { "p", "compute expression", cmd_p },
  { "w", "add new watchpoint", cmd_w },
  { "d", "delete watchpoint by number", cmd_d },
  { "load", "load nemu state, including mem and regs", cmd_load },
  { "save", "save nemu state, including mem and regs", cmd_save },
};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

static int cmd_si(char *args) {
  char *arg = strtok(NULL, " ");
  uint64_t num;

  if (arg == NULL) {num = 1; }
  else {sscanf(arg, "%" SCNu64, &num); }
  // execute(num);
  cpu_exec(num);
  return 0;
}

static int cmd_info(char *args) {
    char *arg = strtok(NULL, " ");
    if (*arg == 'r') {
        isa_reg_display();
    } else if (*arg == 'w') {
        display_wp();
    }
    return 0;
}

static int cmd_x(char *args) {
    unsigned int num = 1;
    vaddr_t addr;
    word_t mem;
    int ret;
    ret = sscanf(args, "%u %x", &num, &addr);
    if (ret < 2) {
        // 如果第一次读取失败，则尝试只读取十六进制数
        sscanf(args, "%x", &addr);
    }
    /*sscanf(args, "%u %x", &num, &addr);*/
    for (int i = 1; i <= num; i++) {
        if (i % 8 == 1) {
            printf("0x%08x :", addr);
        }
        mem = vaddr_read(addr, 1);
        printf("\t0x%02x", mem);
        if (i % 8 == 0 || i == num) {printf("\n"); };
        addr += 1;
    }
    return 0;
}

static int cmd_p(char *args) {
    bool success = true;
    word_t res = expr(args, &success);
    if (success) {
        printf("%u/0x%x\n", res, res);
    } else {
        printf("the expression is wrong, check it\n");
    }
    return 0;
}

static int cmd_w(char *args) {
    new_wp(args);
    return 0;
}

static int cmd_d(char *args) {
    int NO;
    sscanf(args, "%d", &NO);
    free_wp(NO);
    return 0;
}

#define FILE_PATH_LENGTH 128
static int cmd_save(char *args) {
    size_t len = strlen(args);
    char filepath[FILE_PATH_LENGTH ];
    strcpy(filepath, getenv("NEMU_HOME"));
    strcat(filepath, "/src/monitor/nemu_history");

    if (len > 128 - strlen(filepath)) {
        printf("filename too long!\n");
        return 0;
    }
    strcat(filepath, args);
    FILE *fp = fopen(filepath, "w");
    save_mem(fp);
    save_regs(fp);
    //save_mem(fp);
    printf("snapshot saved\n");
    fclose(fp);
    return 0;
}

#define FILE_PATH_LENGTH 128
static int cmd_load(char *args) {
    size_t len = strlen(args);
    char filepath[FILE_PATH_LENGTH ];
    strcpy(filepath, getenv("NEMU_HOME"));
    strcat(filepath, "/src/monitor/nemu_history");

    if (len > 128 - strlen(filepath)) {
        printf("filename too long!\n");
        return 0;
    }

    strcat(filepath, args);
    FILE *fp = fopen(filepath, "r");
    load_mem(fp);
    load_regs(fp);
    //load_mem(fp);
    printf("snapshot loaded\n");
    fclose(fp);
    return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }
    if (qflag == 1) {break; }
    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
