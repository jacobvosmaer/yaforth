
#include <stdarg.h>
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
  int stackp, rstackp, compiling;
  int stack[1024], rstack[1024];
  struct entry *latest, *internal, dict[1024];
  char *error, *token;
} state;

struct {
  int next, current;
} vm;

int mem[8192], nmem;

struct entry *find(struct entry *start, char *word) {
  struct entry *de;
  for (de = start; de >= state.dict; de--)
    if (!(de->flags & F_HIDDEN) && !strcmp(de->word, word))
      return de;
  return 0;
}

void addhere(int x) {
  assert(nmem < nelem(mem));
  mem[nmem++] = x;
}

int asnum(char *token, int *out) {
  char *end = 0;
  *out = strtol(token, &end, 10);
  return !*end;
}

void compile(struct entry *start, char *word) {
  int x;
  struct entry *de = find(start, word);
  if (de) {
    addhere(de - state.dict);
  } else if (asnum(word, &x)) {
    compile(start, "lit");
    addhere(x);
  } else {
    assert(0);
  }
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
  while (n < sizeof(tokbuf)) {
    ch = getchar();
    if (ch == EOF) {
      if (n) {
        tokbuf[n] = 0;
        return tokbuf;
      } else {
        return 0;
      }
    } else if (space(ch) && n) {
      /* keeping ch in the input buffer allows us to discard the rest of the
       * line later if needed */
      ungetc(ch, stdin);
      tokbuf[n] = 0;
      return tokbuf;
    } else if (!space(ch)) {
      tokbuf[n++] = ch;
    }
  }
  assert(0);
}

void next(void) { vm.current = mem[vm.next++]; }

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

void tor(void) {
  int x;
  if (stackpop(&x))
    rstackpush(x);
  next();
}

void fromr(void) {
  stackpush(rstackpop());
  next();
}

void rspstore(void) {
  stackpop(&state.rstackp);
  next();
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

void endcompiling(void) {
  compile(state.internal, "exit");
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

void docol(void);

void startcompiling(void) {
  if (state.latest == endof(state.dict) - 1) {
    state.error = "no room left in dictionary";
  } else {
    state.compiling = 1;
    state.latest++;
    assert(state.token = gettoken());
    assert(state.latest->word = Strdup(state.token));
    state.latest->flags = F_HIDDEN;
    state.latest->func = docol;
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
  compile(state.internal, "branch0");
  stackpush(nmem++);
  next();
}

void compilethen(void) {
  int x;
  if (stackpop(&x)) { /* retrieve start of if body */
    assert(x >= 0 && x < nmem);
    /* compute and store relative jump offset */
    mem[x] = nmem - x;
  }
  next();
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

void lit(void) {
  stackpush(mem[vm.next++]);
  next();
}

void branch(void) {
  vm.next++;
  vm.next += mem[vm.next] - 1;
  next();
}

void branch0(void) {
  int x;
  if (stackpop(&x) && !x)
    vm.next += mem[vm.next];
  next();
}

void docol(void) {
  struct entry *de = state.dict + vm.current;
  assert(de >= state.dict && de <= state.latest);
  rstackpush(vm.next);
  vm.next = de->def - mem;
  next();
}

void interpret(void) {
  int x, ch;
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
      addhere(i);
    } else {
      /* Instead of calling next, jump to word de, and let its next load word in
       * the current sequence (usually the next word in quit). */
      vm.current = i;
      return;
    }
  } else if (asnum(state.token, &x)) {
    if (state.compiling) {
      compile(state.internal, "lit");
      addhere(x);
    } else {
      stackpush(x);
    }
  } else {
    state.error = "unknown word";
  }
  if (state.error) {
    printf("  error: %s\n", state.error);
    if (state.token)
      printf("  token: %s\n", state.token);
    state.error = 0;
    state.compiling = 0;
    while ((ch = getchar())) { /* discard rest of line */
      if (ch == EOF)
        exit(0);
      else if (ch == '\n')
        break;
    }
  }
  next();
}

void exit_(void) {
  vm.next = rstackpop();
  next();
}

void defword(char *word, int flags, ...) {
  va_list ap;
  struct entry *de = state.latest + 1;
  char *w;
  assert(de < endof(state.dict));
  assert(de->word = Strdup(word));
  de->flags = flags | F_HIDDEN;
  de->def = mem + nmem;
  de->func = docol;
  va_start(ap, flags);
  while (w = va_arg(ap, char *), w)
    compile(de, w);
  va_end(ap);
  de->flags &= ~F_HIDDEN;
  state.latest = de;
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
                             {".s", stackprint},
                             {"interpret", interpret},
                             {"exit", exit_},
                             {"lit", lit},
                             {"branch0", branch0},
                             {">r", tor},
                             {"r>", fromr},
                             {"rsp!", rspstore},
                             {"branch", branch}};
  memset(&state, 0, sizeof(state));
  assert(sizeof(initdict) <= sizeof(state.dict));
  memmove(state.dict, initdict, sizeof(initdict));
  state.latest = state.dict + nelem(initdict) - 1;
  defword("rot", 0, ">r", "swap", "r>", "swap", "exit", 0);
  defword("over", 0, ">r", "dup", "r>", "swap", "exit", 0);
  defword("quit", 0, "0", "rsp!", "interpret", "branch", "-2", 0);
  defword("cr", 0, "10", "emit", "exit", 0);
  state.internal = state.latest;
}

int main(void) {
  struct entry *quit;
  initState();
  assert(quit = find(state.internal, "quit"));
  vm.current = quit - state.dict;
  vm.next = quit->def - mem;
  while (1)
    state.dict[vm.current].func();
  return 0;
}
