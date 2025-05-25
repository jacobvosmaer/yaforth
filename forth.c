#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define nelem(x) (sizeof(x) / sizeof(*(x)))
#define endof(x) ((x) + nelem(x))
#define assert(x)                                                              \
  while (!(x))                                                                 \
  __builtin_trap()

enum { F_IMMEDIATE = 1 << 0, F_COMPILE = 1 << 1, F_NOCOMPILE = 1 << 2 };

struct entry {
  char *word;
  void (*func)(void);
  int flags;
  int *def;
};

struct {
  int stackp, ndict, recursive;
  int stack[1024];
  struct entry *compiling, dict[1024];
  char *error, *token;
} state;

int mem[8192], nmem;

void entryreset(struct entry *e) {
  struct entry empty = {0};
  *e = empty;
}

char tokbuf[256];
int space(int ch) { return ch == ' ' || ch == '\n'; }
char *gettoken(void) {
  int ch, n = 0;
  if (state.error) {
    printf("  error: %s\n", state.error);
    if (state.token)
      printf("  token: %s\n", state.token);
    state.error = 0;
    if (state.compiling) {
      entryreset(state.compiling);
      state.compiling = 0;
    }
    state.recursive = 0;
    while ((ch = getchar())) { /* discard rest of line */
      if (ch == EOF)
        return 0;
      else if (ch == '\n')
        break;
    }
  }
  while (n < sizeof(tokbuf)) {
    ch = getchar();
    if (ch == EOF) {
      return 0;
    } else if (space(ch) && n) {
      tokbuf[n] = 0;
      if (ch == '\n') /* trigger printing of state.error */
        ungetc(ch, stdin);
      return tokbuf;
    } else if (ch == '\n') {
      puts("  ok");
    } else if (!space(ch)) {
      tokbuf[n++] = ch;
    }
  }
  assert(0);
}

void defprint(int *def, int deflen) {
  fprintf(stderr, "def:");
  while (deflen-- > 0)
    fprintf(stderr, " %d", *def++);
  fputc('\n', stderr);
}

void stackprint(void) {
  int i;
  printf("<%d>", state.stackp);
  for (i = 0; i < state.stackp; i++)
    printf(" %d", state.stack[i]);
}

int stackpop(int *x) {
  if (state.stackp <= 0) {
    state.error = "stack empty";
    return 0;
  }
  *x = state.stack[--state.stackp];
  return 1;
}

void stackpush(int x) {
  if (state.stackp < nelem(state.stack))
    state.stack[state.stackp++] = x;
  else
    state.error = "stack overflow";
}

void print(void) {
  int x;
  if (stackpop(&x))
    printf(" %d", x);
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
      state.error = "division by zero";
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

enum { DEFNUM = -1, DEFJUMPZ = -2, DEFEND = -3 };

void endcompiling(void) {
  mem[nmem++] = DEFEND;
  state.compiling = 0;
  state.recursive = 0;
  state.ndict++;
}

char *Strdup(char *s) {
  char *p = (char *)(mem + nmem);
  int len = strlen(s), mask = sizeof(*mem) - 1,
      nints = ((len + 1 + mask) & ~mask) / sizeof(*mem);
  if ((nelem(mem) - nmem) < nints)
    return 0;
  memmove(p, s, len);
  memset(p + len, 0, nints * sizeof(int) - len);
  nmem += nints;
  return p;
}

void startcompiling(void) {
  if (state.ndict == nelem(state.dict)) {
    state.error = "no room left in dictionary";
  } else {
    state.compiling = state.dict + state.ndict;
    assert(state.token = gettoken());
    assert(state.dict[state.ndict].word = Strdup(state.token));
    state.compiling->def = mem + nmem;
  }
}

void emit(void) {
  int x;
  if (stackpop(&x))
    putchar(x);
}

void immediate(void) {
  state.dict[state.ndict - !state.compiling].flags |= F_IMMEDIATE;
}

void compileif(void) {
  mem[nmem++] = DEFJUMPZ;
  stackpush(nmem++);
}

void compilethen(void) {
  int x;
  /* retrieve start of if body */
  if (!stackpop(&x))
    return;
  assert(x >= 0 && x < nmem);
  /* compute and store relative jump offset */
  mem[x] = nmem - x - 1;
}

void equal(void) {
  int x, y;
  if (stackpop(&y) && stackpop(&x))
    stackpush(x == y);
}

void greaterthan(void) {
  int x, y;
  if (stackpop(&y) && stackpop(&x))
    stackpush(x > y);
}

void recursive(void) { state.recursive = 1; }

void swap(void) {
  int x, y;
  if (stackpop(&y) && stackpop(&x)) {
    stackpush(y);
    stackpush(x);
  }
}

void drop(void) {
  int x;
  stackpop(&x);
}

void over(void) {
  int x, y;
  if (stackpop(&y) && stackpop(&x)) {
    stackpush(x);
    stackpush(y);
    stackpush(x);
  }
}

void initState(void) {
  struct entry initdict[] = {{".", print},
                             {"+", add},
                             {"-", sub},
                             {"*", mul},
                             {"/", divi},
                             {"clr", clr},
                             {"dup", dup},
                             {";", endcompiling, F_IMMEDIATE | F_COMPILE},
                             {":", startcompiling, F_NOCOMPILE},
                             {"emit", emit},
                             {"immediate", immediate, F_IMMEDIATE},
                             {"=", equal},
                             {"if", compileif, F_IMMEDIATE | F_COMPILE},
                             {"then", compilethen, F_IMMEDIATE | F_COMPILE},
                             {">", greaterthan},
                             {"recursive", recursive, F_IMMEDIATE | F_COMPILE},
                             {"swap", swap},
                             {"drop", drop},
                             {"over", over},
                             {".s", stackprint}};
  memset(&state, 0, sizeof(state));
  assert(sizeof(initdict) <= sizeof(state.dict));
  memmove(state.dict, initdict, sizeof(initdict));
  state.ndict = nelem(initdict);
}

void docol(int *def) {
  for (; *def > DEFEND; def++) {
    if (*def == DEFNUM) {
      stackpush(*++def);
    } else if (*def == DEFJUMPZ) {
      int x;
      def++;
      if (stackpop(&x) && !x) {
        def += *def;
      }
    } else {
      struct entry *de = state.dict + *def;
      assert(de >= state.dict && de < state.dict + state.ndict);
      if (de->func)
        de->func();
      else
        docol(de->def);
    }
  }
}

int asnum(char *token, int *out) {
  char *end = 0;
  *out = strtol(token, &end, 10);
  return !*end;
}

int main(void) {
  initState();
  while ((state.token = gettoken())) {
    int x, i;
    for (i = state.ndict - 1 + state.recursive; i >= 0; i--)
      if (!strcmp(state.dict[i].word, state.token))
        break;
    if (i >= 0) { /* token found in dict */
      struct entry *de = state.dict + i;
      if (state.compiling && de->flags & F_NOCOMPILE) {
        state.error = "word cannot be used while compiling";
      } else if (!state.compiling && de->flags & F_COMPILE) {
        state.error = "word can only be used while compiling";
      } else if (state.compiling && !(de->flags & F_IMMEDIATE)) {
        mem[nmem++] = i;
      } else {
        int def[2];
        def[0] = i;
        def[1] = DEFEND;
        docol(def);
      }
    } else if (asnum(state.token, &x)) {
      if (state.compiling) {
        mem[nmem++] = DEFNUM;
        mem[nmem++] = x;
      } else {
        stackpush(x);
      }
    } else {
      state.error = "unknown word";
    }
  }
  return 0;
}
