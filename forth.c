
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define nelem(x) (sizeof(x) / sizeof(*(x)))
#define endof(x) ((x) + nelem(x))
#define assert(x)                                                              \
  while (!(x))                                                                 \
  __builtin_trap()

enum { F_IMMEDIATE = 1 << 0, F_HIDDEN = 1 << 1 };

struct entry {
  char *word;
  void (*func)(void);
  int flags;
  int *def;
} *dictlatest, dict[1024];

int compiling;

int stack[1024], stackp, rstack[1024], rstackp;

struct {
  int next, current;
} vm;

int mem[8192], nmem;

char wordbuf[256], *error;

void next(void) { vm.current = mem[vm.next++]; }

struct entry *find(struct entry *start, char *word) {
  struct entry *de;
  for (de = start; de >= dict; de--)
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
    addhere(de - dict);
  } else if (asnum(word, &x)) {
    addhere(x);
  } else {
    assert(0);
  }
}

void rstackpush(int x) {
  assert(rstackp < nelem(rstack));
  rstack[rstackp++] = x;
}

int rstackpop(void) {
  assert(rstackp > 0);
  return rstack[--rstackp];
}

int space(int ch) { return ch == ' ' || ch == '\n'; }

void gettoken(void) {
  int ch, n = 0;
  while (n < sizeof(wordbuf)) {
    ch = getchar();
    if (ch == EOF) {
      wordbuf[n] = 0;
      return;
    } else if (space(ch) && n) {
      /* keeping ch in the input buffer allows us to discard the rest of the
       * line later if needed */
      ungetc(ch, stdin);
      wordbuf[n] = 0;
      return;
    } else if (!space(ch)) {
      wordbuf[n++] = ch;
    }
  }
  assert(0);
}

void word(void) {
  gettoken();
  next();
}

void stackprint(void) {
  int i;
  printf("<%d>", stackp);
  for (i = 0; i < stackp; i++)
    printf(" %d", stack[i]);
  next();
}

int stackpop(int *x) {
  if (stackp <= 0) {
    error = "stack empty";
    return 0;
  }
  *x = stack[--stackp];
  return 1;
}

void stackpush(int x) {
  if (stackp < nelem(stack))
    stack[stackp++] = x;
  else
    error = "stack overflow";
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
  stackpop(&rstackp);
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
      error = "division by zero";
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
  stackp = 0;
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

void docol(void) {
  struct entry *de = dict + vm.current;
  assert(de >= dict && de <= dictlatest);
  rstackpush(vm.next);
  vm.next = de->def - mem;
  next();
}

void create_(void) {
  if (dictlatest == endof(dict) - 1) {
    error = "no room left in dictionary";
  } else {
    dictlatest++;
    assert(*wordbuf);
    assert(dictlatest->word = Strdup(wordbuf));
    dictlatest->flags = 0;
    dictlatest->func = docol;
    dictlatest->def = mem + nmem;
  }
}

void create(void) {
  create_();
  next();
}

void latest(void) {
  stackpush(dictlatest - dict);
  next();
}

void hidden(void) {
  int i;
  if (stackpop(&i))
    dict[i].flags ^= F_HIDDEN;
  next();
}

void hiddenset(void) {
  int i;
  if (stackpop(&i))
    dict[i].flags |= F_HIDDEN;
  next();
}

void hiddenclr(void) {
  int i;
  if (stackpop(&i))
    dict[i].flags &= ~F_HIDDEN;
  next();
}

void emit(void) {
  int x;
  if (stackpop(&x))
    putchar(x);
  next();
}

void immediate(void) {
  dictlatest->flags |= F_IMMEDIATE;
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
  dictlatest->flags &= ~F_HIDDEN;
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
  vm.next += mem[vm.next];
  next();
}

void branch0(void) {
  int x;
  if (stackpop(&x) && !x)
    vm.next += mem[vm.next];
  else
    vm.next++;
  next();
}

void here(void) {
  stackpush(nmem);
  next();
}

void comma(void) {
  assert(nmem < nelem(mem));
  stackpop(mem + nmem++);
  next();
}

void store(void) {
  int addr, x;
  if (stackpop(&addr) && stackpop(&x)) {
    assert(addr >= 0 && addr < nelem(mem));
    mem[addr] = x;
  }
  next();
}

void fetch(void) {
  int addr;
  if (stackpop(&addr)) {
    assert(addr >= 0 && addr < nelem(mem));
    stackpush(mem[addr]);
  }
  next();
}

void interpret(void) {
  int x, ch;
  struct entry *de;
  if (gettoken(), !*wordbuf)
    exit(0);
  if (de = find(dictlatest, wordbuf), de >= dict) {
    int i = de - dict;
    if (compiling && !(de->flags & F_IMMEDIATE)) {
      addhere(i);
    } else {
      /* Do not call next(), jump straight to i. Someone else will call next()
       * eventually. */
      vm.current = i;
      return;
    }
  } else if (asnum(wordbuf, &x)) {
    if (compiling) {
      compile(dictlatest, "lit");
      addhere(x);
    } else {
      stackpush(x);
    }
  } else {
    error = "unknown word";
  }
  if (error) {
    printf("  error: %s\n", error);
    if (*wordbuf)
      printf("  token: %s\n", wordbuf);
    error = 0;
    compiling = 0;
    while (ch = getchar(), ch != '\n') /* discard rest of line */
      if (ch == EOF)
        exit(0);
  }
  next();
}

void exit_(void) {
  vm.next = rstackpop();
  next();
}

void lbrac(void) {
  compiling = 0;
  next();
}

void rbrac(void) {
  compiling = 1;
  next();
}

void tick(void) {
  stackpush(mem[vm.next++]);
  next();
}

void defword(char *word, int flags, ...) {
  va_list ap;
  struct entry *de = dictlatest + 1;
  char *w;
  assert(de < endof(dict));
  assert(de->word = Strdup(word));
  de->flags = flags | F_HIDDEN;
  de->def = mem + nmem;
  de->func = docol;
  va_start(ap, flags);
  while (w = va_arg(ap, char *), w)
    compile(de, w);
  va_end(ap);
  de->flags &= ~F_HIDDEN;
  dictlatest = de;
}

void initState(void) {
  struct entry initdict[] = {
      {"exit", exit_},
      {"branch0", branch0},
      {".", print},
      {"+", add},
      {"-", sub},
      {"*", mul},
      {"/", divi},
      {"clr", clr},
      {"dup", dup},
      {"emit", emit},
      {"immediate", immediate, F_IMMEDIATE},
      {"=", equal},
      {">", greaterthan},
      {"recursive", recursive, F_IMMEDIATE},
      {"swap", swap},
      {"drop", drop},
      {".s", stackprint},
      {"interpret", interpret},
      {"lit", lit},
      {">r", tor},
      {"r>", fromr},
      {"rsp!", rspstore},
      {"branch", branch},
      {"here", here},
      {",", comma},
      {"!", store},
      {"@", fetch},
      {"[", lbrac, F_IMMEDIATE},
      {"]", rbrac},
      {"word", word},
      {"create", create},
      {"latest", latest},
      {"hidden", hidden},
      {"hiddenset", hiddenset},
      {"hiddenclr", hiddenclr},
      {"'", tick},
  };
  assert(sizeof(initdict) <= sizeof(dict));
  memmove(dict, initdict, sizeof(initdict));
  dictlatest = dict + nelem(initdict) - 1;
  defword("rot", 0, ">r", "swap", "r>", "swap", "exit", 0);
  defword("over", 0, ">r", "dup", "r>", "swap", "exit", 0);
  defword("quit", 0, "lit", "0", "rsp!", "interpret", "branch", "-2", 0);
  defword("cr", 0, "lit", "10", "emit", "exit", 0);
  defword(":", 0, "word", "create", "latest", "hiddenset", "]", "exit", 0);
  defword(";", F_IMMEDIATE, "'", "exit", ",", "latest", "hiddenclr", "[",
          "exit", 0);
  defword("if", F_IMMEDIATE, "'", "branch0", ",", "here", "lit", "0", ",",
          "exit", 0);
  defword("then", F_IMMEDIATE, "dup", "here", "swap", "-", "swap", "!", "exit",
          0);
}

int main(void) {
  struct entry *quit;
  initState();
  assert(quit = find(dictlatest, "quit"));
  vm.current = quit - dict;
  vm.next = quit->def - mem;
  while (1)
    dict[vm.current].func();
  return 0;
}
