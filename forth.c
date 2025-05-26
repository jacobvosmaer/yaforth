#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define nelem(x) (sizeof(x) / sizeof(*(x)))
#define endof(x) ((x) + nelem(x))
#define assert(x)                                                              \
  while (!(x))                                                                 \
  __builtin_trap()

enum {
  F_IMMEDIATE = 1 << 0,
  F_COMPILE = 1 << 1,
  F_NOCOMPILE = 1 << 2,
  F_HIDDEN = 1 << 3
};

struct entry {
  char *word;
  void (*func)(void);
  int flags;
  int *def;
};

struct {
  int stackp, rstackp, pc, compiling;
  int stack[1024], rstack[1024];
  struct entry *latest, *internal, dict[1024];
  char *error, *token;
} state;

int mem[8192], nmem = 2; /* reserve space for interpret */

struct entry *find(struct entry *start, char *word) {
  struct entry *de;
  for (de = start; de >= state.dict; de--)
    if (!(de->flags & F_HIDDEN) && !strcmp(de->word, word))
      return de;
  return 0;
}

void entryreset(struct entry *e) {
  struct entry empty = {0};
  *e = empty;
}

void rstackpush(int x) {
  assert(state.rstackp < nelem(state.rstack));
  state.rstack[state.rstackp++] = x;
}

int rstackpop(void) {
  assert(state.rstackp > 0);
  return state.rstack[--state.rstackp];
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
      entryreset(state.latest--);
      state.compiling = 0;
    }
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

void next(void) { state.pc++; }

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
  next();
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
  next();
}

void add(void) {
  int x, y;
  if (stackpop(&y) && stackpop(&x))
    stackpush(x + y);
  next();
}

void sub(void) {
  int x, y;
  if (stackpop(&y) && stackpop(&x))
    stackpush(x - y);
  next();
}

void mul(void) {
  int x, y;
  if (stackpop(&y) && stackpop(&x))
    stackpush(x * y);
  next();
}

void divi(void) {
  int x, y;
  if (stackpop(&y) && stackpop(&x)) {
    if (y)
      stackpush(x / y);
    else
      state.error = "division by zero";
  }
  next();
}

void dup(void) {
  int x;
  if (stackpop(&x)) {
    stackpush(x);
    stackpush(x);
  }
  next();
}

void clr(void) {
  state.stackp = 0;
  next();
}

enum { DEFJUMPZ = -2, DEFEND = -3 };

void endcompiling(void) {
  struct entry *exit = find(state.internal, "exit");
  assert(exit);
  assert(nmem < nelem(mem) - 2);
  mem[nmem++] = exit - state.dict;
  mem[nmem++] = DEFEND;
  state.latest->flags &= ~F_HIDDEN;
  state.compiling = 0;
  next();
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
  if (state.latest == endof(state.dict) - 1) {
    state.error = "no room left in dictionary";
  } else {
    state.compiling = 1;
    state.latest++;
    state.latest->flags = F_HIDDEN;
    assert(state.token = gettoken());
    assert(state.latest->word = Strdup(state.token));
    state.latest->def = mem + nmem;
  }
  next();
}

void emit(void) {
  int x;
  if (stackpop(&x))
    putchar(x);
  next();
}

void immediate(void) {
  state.latest->flags |= F_IMMEDIATE;
  next();
}

void compileif(void) {
  mem[nmem++] = DEFJUMPZ;
  stackpush(nmem++);
  next();
}

void compilethen(void) {
  int x;
  next();
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
  next();
}

void greaterthan(void) {
  int x, y;
  if (stackpop(&y) && stackpop(&x))
    stackpush(x > y);
  next();
}

void recursive(void) {
  state.latest->flags &= ~F_HIDDEN;
  next();
}

void swap(void) {
  int x, y;
  if (stackpop(&y) && stackpop(&x)) {
    stackpush(y);
    stackpush(x);
  }
  next();
}

void drop(void) {
  int x;
  stackpop(&x);
  next();
}

void over(void) {
  int x, y;
  if (stackpop(&y) && stackpop(&x)) {
    stackpush(x);
    stackpush(y);
    stackpush(x);
  }
  next();
}

void lit(void) {
  stackpush(mem[++state.pc]);
  next();
}

void docol(void) {
  while (mem[state.pc] > DEFEND) {
    if (mem[state.pc] == DEFJUMPZ) {
      int x;
      state.pc++;
      if (stackpop(&x) && !x) 
        state.pc += mem[state.pc];
       state.pc++;
    } else {
      struct entry *de = state.dict + mem[state.pc];
      assert(de >= state.dict && de <= state.latest);
      if (de->func) {
        de->func();
      } else {
        rstackpush(state.pc);
        state.pc = de->def - mem;
      }
    }
  }
}

int asnum(char *token, int *out) {
  char *end = 0;
  *out = strtol(token, &end, 10);
  return !*end;
}

void interpret(void) {
  int x;
  struct entry *de;
  if (state.token = gettoken(), !state.token)
    exit(0);
  if (de = find(state.latest, state.token), de >= state.dict) {
    int i = de - state.dict;
    if (state.compiling && de->flags & F_NOCOMPILE) {
      state.error = "word cannot be used while compiling";
    } else if (!state.compiling && de->flags & F_COMPILE) {
      state.error = "word can only be used while compiling";
    } else if (state.compiling && !(de->flags & F_IMMEDIATE)) {
      mem[nmem++] = i;
    } else {
      mem[0] = i;
      mem[1] = DEFEND;
      state.pc = 0;
      docol();
    }
  } else if (asnum(state.token, &x)) {
    if (state.compiling) {
      assert(de = find(state.internal, "lit"));
      mem[nmem++] = de - state.dict;
      mem[nmem++] = x;
    } else {
      stackpush(x);
    }
  } else {
    state.error = "unknown word";
  }
}

void exit_(void) {
  state.pc = rstackpop();
  next();
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
                             {".s", stackprint},
                             {"interpret", interpret},
                             {"exit", exit_},
                             {"lit", lit}};
  memset(&state, 0, sizeof(state));
  assert(sizeof(initdict) <= sizeof(state.dict));
  memmove(state.dict, initdict, sizeof(initdict));
  state.latest = state.dict + nelem(initdict) - 1;
  state.internal = state.latest;
}

int main(void) {
  initState();
  while (1)
    interpret();
  return 0;
}
