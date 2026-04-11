#include "monitor/watchpoint.h"
#include "monitor/expr.h"

#define NR_WP 32

static WP wp_pool[NR_WP];
static WP *head, *free_;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = &wp_pool[i + 1];
  }
  wp_pool[NR_WP - 1].next = NULL;

  head = NULL;
  free_ = wp_pool;
}

WP* new_wp(char *expr_str) {
  WP *wp;
  bool success = false;

  if (expr_str == NULL) {
    printf("Usage: w EXPR\n");
    return NULL;
  }

  if (free_ == NULL) {
    printf("No free watchpoint.\n");
    return NULL;
  }

  wp = free_;
  free_ = free_->next;

  wp->next = head;
  head = wp;

  strncpy(wp->expr, expr_str, sizeof(wp->expr) - 1);
  wp->expr[sizeof(wp->expr) - 1] = '\0';

  wp->old_val = expr(wp->expr, &success);
  if (!success) {
    head = wp->next;
    wp->next = free_;
    free_ = wp;
    printf("Bad expression: %s\n", expr_str);
    return NULL;
  }

  printf("Watchpoint %d: %s = %u (0x%08x)\n", wp->NO, wp->expr, wp->old_val, wp->old_val);
  return wp;
}

bool free_wp(int no) {
  WP *prev = NULL;
  WP *cur = head;

  while (cur != NULL) {
    if (cur->NO == no) {
      if (prev == NULL) {
        head = cur->next;
      }
      else {
        prev->next = cur->next;
      }

      cur->next = free_;
      free_ = cur;
      return true;
    }
    prev = cur;
    cur = cur->next;
  }

  return false;
}

void list_watchpoints() {
  WP *cur = head;

  if (cur == NULL) {
    printf("No watchpoints.\n");
    return;
  }

  printf("Num\tValue\t\tExpr\n");
  while (cur != NULL) {
    printf("%d\t0x%08x\t%s\n", cur->NO, cur->old_val, cur->expr);
    cur = cur->next;
  }
}

bool check_watchpoints() {
  WP *cur = head;
  bool triggered = false;

  while (cur != NULL) {
    bool success = false;
    uint32_t new_val = expr(cur->expr, &success);

    if (!success) {
      printf("Watchpoint %d expression became invalid: %s\n", cur->NO, cur->expr);
      cur = cur->next;
      continue;
    }

    if (new_val != cur->old_val) {
      printf("Watchpoint %d triggered: %s\n", cur->NO, cur->expr);
      printf("Old value = %u (0x%08x)\n", cur->old_val, cur->old_val);
      printf("New value = %u (0x%08x)\n", new_val, new_val);
      cur->old_val = new_val;
      triggered = true;
    }

    cur = cur->next;
  }

  return triggered;
}

