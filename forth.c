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
  int deflen;
};

struct {
  int stackp, ndict, recursive;
  int stack[1024];
  struct entry *compiling, dict[1024];
  char *error, *token;
} state;

char tokbuf[256];
int space(int ch) { return ch == ' ' || ch == '\n'; }
char *gettoken(void) {
  int ch, n = 0;
  if (state.error) {
    printf("  error: %s\n", state.error);
    if (state.token)
      printf("  token: %s\n", state.token);
    state.error = 0;
    state.compiling = 0;
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

void endcompiling(void) {
  state.compiling = 0;
  state.recursive = 0;
  state.ndict++;
}

void startcompiling(void) {
  if (state.ndict == nelem(state.dict)) {
    state.error = "no room left in dictionary";
  } else {
    state.compiling = state.dict + state.ndict;
    assert(state.token = gettoken());
    assert(state.dict[state.ndict].word = strdup(state.token));
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

enum { DEFNUM = -1, DEFJUMPNZ = -2 };

void defgrow(struct entry *ne, int n) {
  ne->deflen += n;
  assert(ne->def = realloc(ne->def, ne->deflen * sizeof(*ne->def)));
}

void compileif(void) {
  defgrow(state.compiling, 2);
  state.compiling->def[state.compiling->deflen - 2] = DEFJUMPNZ;
  stackpush(state.compiling->deflen - 1);
}

void compilethen(void) {
  int x;
  if (!stackpop(&x))
    return;
  assert(x >= 0 && x < state.compiling->deflen);
  state.compiling->def[x] = state.compiling->deflen - x - 1;
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
                             {"recursive", recursive, F_IMMEDIATE | F_COMPILE}};
  memset(&state, 0, sizeof(state));
  assert(sizeof(initdict) <= sizeof(state.dict));
  memmove(state.dict, initdict, sizeof(initdict));
  state.ndict = nelem(initdict);
}

void interpret(int *def, int deflen) {
  int i;
  for (i = 0; i < deflen; i++) {
    if (def[i] == DEFNUM) {
      assert(++i < deflen);
      stackpush(def[i]);
    } else if (def[i] == DEFJUMPNZ) {
      int x;
      assert(++i < deflen);
      if (stackpop(&x) && !x) {
        i += def[i];
        assert(i >= 0 && i < deflen);
      }
    } else {
      struct entry *de = state.dict + def[i];
      assert(de >= state.dict && de < state.dict + state.ndict);
      if (de->func)
        de->func();
      else
        interpret(de->def, de->deflen);
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
    int num[2], i;
    for (i = state.ndict - 1 + state.recursive; i >= 0; i--) {
      struct entry *de = state.dict + i;
      if (!strcmp(de->word, state.token)) {
        if (state.compiling && de->flags & F_NOCOMPILE) {
          state.error = "word cannot be used while compiling";
        } else if (!state.compiling && de->flags & F_COMPILE) {
          state.error = "word can only be used while compiling";
        } else if (state.compiling && !(de->flags & F_IMMEDIATE)) {
          defgrow(state.compiling, 1);
          state.compiling->def[state.compiling->deflen - 1] = i;
        } else {
          interpret(&i, 1);
        }
        break;
      }
    }
    if (i >= 0)
      continue;
    if (asnum(state.token, &num[1])) {
      num[0] = DEFNUM;
      if (state.compiling) {
        defgrow(state.compiling, nelem(num));
        memmove(state.compiling->def + state.compiling->deflen - nelem(num),
                num, sizeof(num));
      } else {
        interpret(num, nelem(num));
      }
    } else {
      state.error = "unknown word";
    }
  }
  return 0;
}
