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

struct entry {
  char *word;
  void (*func)(void);
  int *def;
  int deflen;
  int immediate;
};

struct {
  int stackp, compiling, ndict;
  int stack[1024];
  struct entry dict[1024];
} state;

void stackprint(void) {
  int i;
  printf("<%d>", state.stackp);
  for (i = 0; i < state.stackp; i++)
    printf(" %d", state.stack[i]);
}

int stackpop(int *x) {
  if (state.stackp <= 0) {
    warnx("stack empty");
    return 0;
  }
  *x = state.stack[--state.stackp];
  return 1;
}

void stackpush(int x) {
  if (state.stackp < nelem(state.stack))
    state.stack[state.stackp++] = x;
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

void clr(void) { state.stackp = 0; }

void printdef(struct entry *e) {
  int i;
  fprintf(stderr, "def=");
  for (i = 0; i < e->deflen; i++)
    fprintf(stderr, " %d", e->def[i]);
  fputc('\n', stderr);
}

void endcompiling(void) {
  if (state.compiling) {
    state.compiling = 0;
    state.ndict++;
  } else
    warnx("unexpected ;");
}

void startcompiling(void) {
  char *token;
  if (state.compiling) {
    warnx("already compiling");
    return;
  }
  if (state.ndict == nelem(state.dict)) {
    warnx("no room left in dictionary");
    return;
  }

  state.compiling = 1;
  assert(token = gettoken());
  assert(state.dict[state.ndict].word = strdup(token));
}

void initState(void) {
  struct entry *de, initdict[] = {
                        {".", print},
                        {"+", add},
                        {"-", sub},
                        {"*", mul},
                        {"/", divi},
                        {"clr", clr},
                        {"dup", dup},
                        {";", endcompiling, 0, 0, 1},
                        {":", startcompiling},
                    };
  assert(nelem(initdict) <= nelem(state.dict));
  for (de = initdict; de < endof(initdict); de++)
    state.dict[state.ndict++] = *de;
}

void interpret(int *def, int deflen) {
  while (deflen--) {
    struct entry *de;
    int n = *def++;
    assert(n >= 0 && n < state.ndict);
    de = state.dict + n;
    if (de->func)
      de->func();
    else
      interpret(de->def, de->deflen);
  }
}

int main(void) {
  char *token;
  initState();
  while ((token = gettoken())) {
    int x, i;
    for (i = state.ndict - 1; i >= 0; i--) {
      struct entry *de = state.dict + i;
      if (!strcmp(de->word, token)) {
        if (state.compiling && !de->immediate) {
          struct entry *ne = state.dict + state.ndict;
          assert(ne->def = realloc(ne->def, ++(ne->deflen) * sizeof(*ne->def)));
          ne->def[ne->deflen - 1] = i;
        } else {
          interpret(&i, 1);
        }
        break;
      }
    }
    if (i >= 0)
      continue;
    if (asnum(token, &x))
      stackpush(x);
    else
      warnx("unknown token: %s", token);
  }
}
