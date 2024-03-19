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

#include "debug.h"
#include "sdb.h"
#include <stdio.h>
#include <string.h>

#define NR_WP 32
#define NR_EXP 100

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;

  /* TODO: Add more members if necessary */
  char exp[NR_EXP];
  word_t old_value;
  int initialized;
} WP;

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */

void new_wp(char *exp) {
    if (free_ == NULL) {
        Log("there is no free watchpoint, %d all used", NR_WP);
        return;
    }
    WP *head_next = head;
    head = free_;
    free_ = free_->next;
    head->next = head_next;

    // initialize
    head->old_value = 0;
    head->initialized = 0;
    strncpy(head->exp, exp, NR_EXP);
}

void free_wp(int NO) {
    WP *cur = head;
    WP *prev = cur;
    while (cur && cur->NO != NO) {
        prev = cur;
        cur = cur->next;
    }
    if (cur->NO != NO) {
        Log("No %d watchpoint does't exist", NO);
        return;
    }
    prev->next = cur->next;
    if (cur == head)
        head = NULL;

    WP *temp = free_;
    free_ = cur;
    free_->next = temp;
}

void display_wp() {
    WP *cur = head;
    if (cur == NULL)
        Log("No watchpoint");
    printf("%-10s%-50s\n", "NO", "expr");
    while (cur != NULL) {
        printf("%-10d%-50s\n", cur->NO, cur->exp);
        cur = cur->next;
    }
}

bool check_wp() {
    WP *cur = head;
    bool changed = false;
    bool success = true;
    while (cur != NULL) {
        word_t res = expr(cur->exp, &success);
        if (!success) {
            Log("watchpoint %d compute expression wrong", cur->NO);
            continue;
        }
        if (!cur->initialized || res != cur->old_value) {
            if (!changed) 
                printf("%-10s%-50s%-20s%-20s\n", "NO", "expr", "old", "new");
            printf("%-10d%-50s%-20u%-20u\n", cur->NO, cur->exp, cur->old_value, res);
            changed = true;
            cur->initialized = true;
            cur->old_value = res;
        }
        cur = cur->next;
    }
    return changed;
}
