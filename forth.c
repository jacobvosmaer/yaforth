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
  int stackp, compiling, ndict, recursive;
  int stack[1024];
  struct entry dict[1024];
  char *error;
} state;

char tokbuf[256];
int space(int ch) { return ch == ' ' || ch == '\n'; }
char *gettoken(void) {
  int ch, n = 0;
  if (state.error) {
    printf("  error: %s\n", state.error);
    state.error = 0;
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
      if (ch == '\n')
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
  if (state.compiling) {
    if (0) {
      struct entry *ne = state.dict + state.ndict;
      int i;
      fprintf(stderr, "word=%s immediate=%d def=", ne->word, ne->immediate);
      for (i = 0; i < ne->deflen; i++) {
        fprintf(stderr, "%d", ne->def[i]);
        if (i < ne->deflen - 1)
          fputc(',', stderr);
      }
      fputc('\n', stderr);
    }
    state.compiling = 0;
    state.recursive = 0;
    state.ndict++;
  } else
    state.error = "unexpected ;";
}

void startcompiling(void) {
  char *token;
  if (state.compiling) {
    state.error = "already compiling";
  } else if (state.ndict == nelem(state.dict)) {
    state.error = "no room left in dictionary";
  } else {
    state.compiling = 1;
    assert(token = gettoken());
    assert(state.dict[state.ndict].word = strdup(token));
  }
}

void emit(void) {
  int x;
  if (stackpop(&x))
    putchar(x);
}

void immediate(void) {
  state.dict[state.ndict - !state.compiling].immediate = 1;
}

enum { DEFNUM = -1, DEFJUMPNZ = -2 };

void defgrow(struct entry *ne, int n) {
  ne->deflen += n;
  assert(ne->def = realloc(ne->def, ne->deflen * sizeof(*ne->def)));
}

void compileif(void) {
  struct entry *ne = state.dict + state.ndict;
  if (!state.compiling) {
    state.error = "if is compile-only";
    return;
  }
  defgrow(ne, 2);
  ne->def[ne->deflen - 2] = DEFJUMPNZ;
  stackpush(ne->deflen - 1);
}

void compilethen(void) {
  struct entry *ne = state.dict + state.ndict;
  int x;
  if (!stackpop(&x))
    return;
  if (!state.compiling) {
    state.error = "then is compile-only";
    return;
  }
  assert(x < ne->deflen);
  ne->def[x] = ne->deflen - x - 1;
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

void recursive(void) {
  if (state.compiling)
    state.recursive = 1;
  else
    state.error = "recursive is compile-only";
}

void initState(void) {
  struct entry *de, initdict[] = {{".", print},
                                  {"+", add},
                                  {"-", sub},
                                  {"*", mul},
                                  {"/", divi},
                                  {"clr", clr},
                                  {"dup", dup},
                                  {";", endcompiling, 0, 0, 1},
                                  {":", startcompiling},
                                  {"emit", emit},
                                  {"immediate", immediate, 0, 0, 1},
                                  {"=", equal},
                                  {"if", compileif, 0, 0, 1},
                                  {"then", compilethen, 0, 0, 1},
                                  {">", greaterthan},
                                  {"recursive", recursive, 0, 0, 1}};
  assert(nelem(initdict) <= nelem(state.dict));
  for (de = initdict; de < endof(initdict); de++)
    state.dict[state.ndict++] = *de;
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

int main(void) {
  char *token;
  initState();
  while ((token = gettoken())) {
    int num[2], i;
    for (i = state.ndict - 1 + state.recursive; i >= 0; i--) {
      struct entry *de = state.dict + i;
      if (!strcmp(de->word, token)) {
        if (state.compiling && !de->immediate) {
          struct entry *ne = state.dict + state.ndict;
          defgrow(ne, 1);
          ne->def[ne->deflen - 1] = i;
        } else {
          interpret(&i, 1);
        }
        break;
      }
    }
    if (i >= 0)
      continue;
    if (asnum(token, num + 1)) {
      num[0] = DEFNUM;
      if (state.compiling) {
        struct entry *ne = state.dict + state.ndict;
        defgrow(ne, nelem(num));
        memmove(ne->def + ne->deflen - nelem(num), num, sizeof(num));
      } else {
        interpret(num, nelem(num));
      }
    } else {
      state.error = "unknown token";
    }
  }
}
