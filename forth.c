#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define nelem(x) (sizeof(x) / sizeof(*(x)))
#define endof(x) ((x) + nelem(x))
#define assert(x)                                                              \
  while (!(x))                                                                 \
  __builtin_trap()

char tokbuf[256];
int space(int ch) { return ch == ' ' || ch == '\n'; }
char *gettoken(void) {
  int ch, n = 0;
  while (n < sizeof(tokbuf)) {
    ch = getchar();
    if (ch == EOF) {
      return 0;
    } else if (space(ch) && n) {
      tokbuf[n] = 0;
      return tokbuf;
    } else if (!space(ch)) {
      tokbuf[n++] = ch;
    }
  }
  assert(0);
}

int asnum(char *token, int *out) {
  char *end = 0;
  *out = strtol(token, &end, 10);
  return !*end;
}

int stack[1024], stackp;

void stackprint(void) {
  int i;
  printf("<%d>", stackp);
  for (i = 0; i < stackp; i++)
    printf(" %d", stack[i]);
}

int stackpop(int *x) {
  if (stackp <= 0) {
    warnx("stack empty");
    return 0;
  }
  *x = stack[--stackp];
  return 1;
}

void stackpush(int x) {
  if (stackp < nelem(stack))
    stack[stackp++] = x;
  else
    warnx("stack overflow");
}

void print(void) {
  int x;
  if (stackpop(&x))
    printf("  %d\n", x);
}

void add(void) {
  int x, y;
  if (stackpop(&y) && stackpop(&x))
    stackpush(x + y);
}

void sub(void) {
  int x, y;
  if (stackpop(&y) && stackpop(&x))
    stackpush(x - y);
}

void mul(void) {
  int x, y;
  if (stackpop(&y) && stackpop(&x))
    stackpush(x * y);
}

void divi(void) {
  int x, y;
  if (stackpop(&y) && stackpop(&x)) {
    if (y)
      stackpush(x / y);
    else
      warnx("division by zero");
  }
}

void dup(void) {
  int x;
  if (stackpop(&x)) {
    stackpush(x);
    stackpush(x);
  }
}

void clr(void) { stackp = 0; }

struct entry {
  char *word;
  void (*func)(void);
} dict[] = {{"print", print}, {"+", add},   {"-", sub},  {"*", mul},
            {"/", divi},      {"clr", clr}, {"dup", dup}};

int main(void) {
  char *token;
  while ((token = gettoken())) {
    struct entry *de;
    int x;
    for (de = endof(dict) - 1; de >= dict; de--) {
      if (!strcmp(de->word, token)) {
        de->func();
        break;
      }
    }
    if (de >= dict)
      continue;
    if (asnum(token, &x))
      stackpush(x);
    else
      warnx("unknown token: %s", token);
  }
}
