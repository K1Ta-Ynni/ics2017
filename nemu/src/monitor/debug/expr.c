#include "nemu.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>
#include <stdlib.h>

enum {
  TK_NOTYPE = 256, TK_EQ, TK_NEQ, TK_NUM, TK_HEX, TK_REG, TK_NEG, TK_DEREF

  /* TODO: Add more token types */

};

static struct rule {
  char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE},    // spaces
  {"0[xX][0-9a-fA-F]+", TK_HEX}, // hexadecimal number
  {"[0-9]+", TK_NUM},   // decimal number
  {"\\$[a-zA-Z][a-zA-Z0-9]*", TK_REG}, // register
  {"!=", TK_NEQ},       // not equal
  {"\\+", '+'},         // plus
  {"==", TK_EQ},        // equal
  {"-", '-'},           // minus
  {"\\*", '*'},         // multiply
  {"/", '/'},           // divide
  {"\\(", '('},         // left parenthesis
  {"\\)", ')'}          // right parenthesis
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]) )

static regex_t re[NR_REGEX];

/* Rules are used for many times.
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

Token tokens[64];
int nr_token;

static bool is_operand(int type) {
  return type == TK_NUM || type == TK_HEX || type == TK_REG || type == ')';
}

static bool reg_str2val(const char *s, uint32_t *val) {
  int i;
  const char *reg = s + 1;

  for (i = 0; i < 8; i ++) {
    if (strcmp(reg, regsl[i]) == 0) {
      *val = reg_l(i);
      return true;
    }
  }

  for (i = 0; i < 8; i ++) {
    if (strcmp(reg, regsw[i]) == 0) {
      *val = reg_w(i);
      return true;
    }
  }

  for (i = 0; i < 8; i ++) {
    if (strcmp(reg, regsb[i]) == 0) {
      *val = reg_b(i);
      return true;
    }
  }

  if (strcmp(reg, "eip") == 0) {
    *val = cpu.eip;
    return true;
  }

  return false;
}

static bool check_parentheses(int p, int q) {
  int i;
  int balance = 0;

  if (tokens[p].type != '(' || tokens[q].type != ')') {
    return false;
  }

  for (i = p; i <= q; i ++) {
    if (tokens[i].type == '(') {
      balance ++;
    }
    else if (tokens[i].type == ')') {
      balance --;
      if (balance == 0 && i < q) {
        return false;
      }
      if (balance < 0) {
        return false;
      }
    }
  }

  return balance == 0;
}

static int precedence(int type) {
  switch (type) {
    case TK_EQ:
    case TK_NEQ: return 1;
    case '+':
    case '-': return 2;
    case '*':
    case '/': return 3;
    default: return 0;
  }
}

static int find_main_operator(int p, int q) {
  int i;
  int balance = 0;
  int op = -1;
  int min_prec = 0;

  for (i = p; i <= q; i ++) {
    if (tokens[i].type == '(') {
      balance ++;
      continue;
    }
    if (tokens[i].type == ')') {
      balance --;
      continue;
    }
    if (balance != 0) {
      continue;
    }

    if (precedence(tokens[i].type) > 0) {
      if (op == -1 || precedence(tokens[i].type) <= min_prec) {
        op = i;
        min_prec = precedence(tokens[i].type);
      }
    }
  }

  return op;
}

static uint32_t eval(int p, int q, bool *success) {
  uint32_t val1, val2;
  int op;

  if (p > q) {
    *success = false;
    return 0;
  }

  if (p == q) {
    uint32_t reg_val;

    if (tokens[p].type == TK_NUM) {
      return strtoul(tokens[p].str, NULL, 10);
    }
    if (tokens[p].type == TK_HEX) {
      return strtoul(tokens[p].str, NULL, 16);
    }
    if (tokens[p].type == TK_REG && reg_str2val(tokens[p].str, &reg_val)) {
      return reg_val;
    }

    *success = false;
    return 0;
  }

  if (check_parentheses(p, q)) {
    return eval(p + 1, q - 1, success);
  }

  op = find_main_operator(p, q);
  if (op != -1) {
    val1 = eval(p, op - 1, success);
    if (!*success) {
      return 0;
    }

    val2 = eval(op + 1, q, success);
    if (!*success) {
      return 0;
    }

    switch (tokens[op].type) {
      case '+': return val1 + val2;
      case '-': return val1 - val2;
      case '*': return val1 * val2;
      case '/':
        if (val2 == 0) {
          *success = false;
          return 0;
        }
        return val1 / val2;
      case TK_EQ: return val1 == val2;
      case TK_NEQ: return val1 != val2;
      default:
        *success = false;
        return 0;
    }
  }

  switch (tokens[p].type) {
    case TK_NEG:
      val1 = eval(p + 1, q, success);
      if (!*success) {
        return 0;
      }
      return -val1;
    case TK_DEREF:
      val1 = eval(p + 1, q, success);
      if (!*success) {
        return 0;
      }
      return vaddr_read(val1, 4);
    default:
      *success = false;
      return 0;
  }
}

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

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch (rules[i].token_type) {
          case TK_NOTYPE:
            break;
          case TK_NUM:
          case TK_HEX:
          case TK_REG:
          case TK_EQ:
          case TK_NEQ:
          case '+':
          case '-':
          case '*':
          case '/':
          case '(':
          case ')':
            Assert(nr_token < sizeof(tokens) / sizeof(tokens[0]), "Expression is too long");
            tokens[nr_token].type = rules[i].token_type;
            if (substr_len >= sizeof(tokens[nr_token].str)) {
              printf("token is too long: %.*s\n", substr_len, substr_start);
              return false;
            }
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            tokens[nr_token].str[substr_len] = '\0';
            nr_token ++;
            break;
          default:
            panic("unknown token type %d", rules[i].token_type);
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  for (i = 0; i < nr_token; i ++) {
    if (tokens[i].type == '-' && (i == 0 || !is_operand(tokens[i - 1].type))) {
      tokens[i].type = TK_NEG;
    }
    else if (tokens[i].type == '*' && (i == 0 || !is_operand(tokens[i - 1].type))) {
      tokens[i].type = TK_DEREF;
    }
  }

  return true;
}

uint32_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  if (nr_token == 0) {
    *success = false;
    return 0;
  }

  *success = true;
  return eval(0, nr_token - 1, success);
}
