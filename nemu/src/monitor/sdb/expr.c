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
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  TK_NOTYPE = 256, TK_EQ, TK_DIGIT, TK_PAREN_LEFT, TK_PAREN_RIGHT,

  /* TODO: Add more token types */

};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE},    // spaces, + 在正则表达式中表示符号至少出现一次,所以是多个空格
  {"\\+", '+'},         // plus
  {"==", TK_EQ},        // equal
  {"\\-", '-'},         // sub
  {"\\*", '*'},         // mul
  {"/", '/'},           // div 
  {"[0-9]+", TK_DIGIT},   // digit, extended regex don't support \d etc, perl regex does
  {"\\(", TK_PAREN_LEFT}, // parenthesis
  {"\\)", TK_PAREN_RIGHT}, // parenthesis right
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* construct our own regex rules and patterns
 * Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        //Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s", i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */
        switch (rules[i].token_type) {
            case TK_DIGIT: 
                if (substr_len > 31) { 
                    Log("error: one digit is too long, should less than 32 characters"); 
                    return false; 
                } else {
                    int j = 0; 
                    for (j = 0; j < substr_len; j++) {
                        tokens[nr_token].str[j] = *(substr_start + j);
                    }
                    tokens[nr_token].str[j] = '\0';
                }
                tokens[nr_token].type = rules[i].token_type;
                nr_token++;
                break;
            case TK_NOTYPE:
                break;
            default:
                tokens[nr_token].type = rules[i].token_type;
                nr_token++;    
        }
        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }
  return true;
}

static bool check_parentheses(int p, int q, bool *success) {
    int type;
    int cur = p;
    int index = 0;
    while(cur <= q) {
        type = tokens[cur].type; 
        if (type == TK_PAREN_LEFT) {
            index++;
        } else if (type == TK_PAREN_RIGHT) {
            index--;
        }
        // (expr)) or )expr( 
        if (index < 0) {
            *success = false;
            return false;
        }
        // (expr) + (expr)
        if (index == 0 && cur != q) {
            return false;
        }
        cur++;
    }
    // ((expr): left parenthesis more than right parenthesis
    if (index == 0)
        return true;
    else {
        *success = false;
        return false;
    }
}

static word_t strtonum(int k, bool *success) {
    // bug fix
    if (tokens[k].type != TK_DIGIT) {
        *success = false;
        Log("when convert string to number, token's type is not digit\n");
        return 0;
    }
    unsigned long val = strtoul(tokens[k].str, NULL, 10);
    // 检查是否发生了溢出或者转换错误
    if ((errno == ERANGE && val == ULONG_MAX) || (errno != 0 && val == 0)) {
        perror("strtoul");
        *success = false;
        return 0;
    }

    // 检查val是否超出uint32_t的范围
    if (val > UINT32_MAX) {
        printf("Value out of range for uint32_t\n");
        return 0;
    }

    return (word_t)val;
}

word_t eval(int p, int q, bool *success) {
    if (!(*success)) {
        return 0;
    }
    if (p > q) {
        *success = false;
        return 0;
    } else if (p == q) {
        return strtonum(p, success);
    } else if (check_parentheses(p, q, success) == true) {
        return eval(p + 1, q - 1, success);
    } else {
        // find main op, and split expression, final compute two expression results with op
        bool plus_sub = false;
        int position = -1;
        char op = '\0';
        int in_parenthesis = 0;
        for (int i = p; i <= q; i++) {
            if (tokens[i].type == TK_PAREN_RIGHT) {
                in_parenthesis -= 1;
                continue;
            } else if (tokens[i].type == TK_PAREN_LEFT) {
                in_parenthesis += 1;
                continue;
            // main op 只会存在于括号之外，因此 in_p !=0 时，直接 pass    
            } else if (in_parenthesis != 0) {
                continue;
            }

            // 判定主op时，加号和减号的优先级高于乘号和除号
            if (!plus_sub && (tokens[i].type == '*' || tokens[i].type == '/')) {
                position = i;
                op = tokens[i].type;
            } else if (tokens[i].type == '-' || tokens[i].type == '+') {
                position = i;
                op = tokens[i].type;
                plus_sub = true;
            }
        }
        word_t val1 = eval(p, position - 1, success);
        word_t val2 = eval(position + 1, q, success);
        
        switch (op) {
            case '+': return val1 + val2;
            case '-': return val1 - val2;
            case '*': return val1 * val2;
            case '/': return val1 / val2;
            default: {
                *success = false;
                return 0;
            }
        }
    }
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  return eval(0, nr_token - 1, success);
}
