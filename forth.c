
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtin.h"

#define nelem(x) (sizeof(x) / sizeof(*(x)))
#define endof(x) ((x) + nelem(x))
#define assert(x)                                                              \
  while (!(x))                                                                 \
  __builtin_trap()
#define offsetof(t, f) (size_t)((char *)&((t *)0)->f - (char *)0)
#define alignof(t)                                                             \
  offsetof(                                                                    \
      struct {                                                                 \
        char c;                                                                \
        t f;                                                                   \
      },                                                                       \
      f)

enum { F_IMMEDIATE = 1 << 0, F_HIDDEN = 1 << 1 };

struct entry {
  void (*func)(void);
  int word, flags, def, next;
} *dictlatest;

int compiling;

int stack[1024], stackp, rstack[1024], rstackp;

struct {
  int next, current;
} vm;

#define memsize 32768
union {
  int alignint;
  struct entry alignentry;
  unsigned char memchar[memsize];
} mem_;

unsigned char *mem = mem_.memchar;
/* Store nmem at logical addres 0. This prevents the first dictionary entry from
 * being stored, allowing us to use 0 as a sentinel for the dictionary linked
 * list. */
int *nmem = &mem_.alignint;

#define memoff(de) ((unsigned char *)(de) - mem)

int intpp(int *x) {
  int old = *x;
  *x += sizeof(int);
  return old;
}

int *intaddr(int addr) {
  assert(!(addr & (sizeof(int) - 1)));
  assert(addr >= 0 && addr < memsize);
  return (int *)(mem + addr);
}

int alignentry(int x) {
  int mask = alignof(struct entry) - 1;
  assert(x < INT_MAX - mask);
  return (x + mask) & ~mask;
}

struct entry *entryaddr(int addr) {
  assert(!(addr & (alignof(struct entry) - 1)));
  assert(addr >= 0 && addr < memsize);
  return (struct entry *)(mem + addr);
}

char *wordaddr(int addr) { return (char *)(mem + addr + sizeof(int)); }

char wordbuf[256], *error;

void next(void) { vm.current = *intaddr(intpp(&vm.next)); }

int find(struct entry *start, char *word) {
  struct entry *de;
  int n = strlen(word);
  for (de = start; memoff(de); de = entryaddr(de->next))
    if (!(de->flags & F_HIDDEN) && n == *intaddr(de->word) &&
        !memcmp(word, wordaddr(de->word), n))
      return memoff(de);
  return 0;
}

void addhere(int x) { *intaddr(intpp(nmem)) = x; }

int asnum(char *token, int *out) {
  char *end = 0;
  *out = strtol(token, &end, 10);
  return !*end;
}

void compile(struct entry *start, char *word) {
  int x;
  int de = find(start, word);
  if (de) {
    addhere(de);
  } else if (asnum(word, &x)) {
    addhere(x);
  } else {
    assert(0);
  }
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

void rstackpush(int x) {
  assert(rstackp < nelem(rstack));
  rstack[rstackp++] = x;
}

int rstackpop(void) {
  assert(rstackp > 0);
  return rstack[--rstackp];
}

int builtinp;

int Getchar(void) {
  return builtinp < nelem(builtinforth) ? builtinforth[builtinp++] : getchar();
}

void Ungetchar(int c) {
  if (builtinp > 0 && builtinp <= nelem(builtinforth))
    builtinforth[--builtinp] = c;
  else
    ungetc(c, stdin);
}

int space(int ch) { return ch == ' ' || ch == '\n'; }

void gettoken(void) {
  int ch, n = 0;
  while (n < sizeof(wordbuf)) {
    ch = Getchar();
    if (ch == EOF) {
      wordbuf[n] = 0;
      return;
    } else if (space(ch) && n) {
      /* keeping ch in the input buffer allows us to discard the rest of the
       * line later if needed */
      Ungetchar(ch);
      wordbuf[n] = 0;
      return;
    } else if (ch == '\\' && !n) {
      while (ch = Getchar(), ch != '\n' && ch != EOF)
        ;
      Ungetchar(ch);
    } else if (!space(ch)) {
      wordbuf[n++] = ch;
    }
  }
  assert(0);
}

void key(void) {
  stackpush(Getchar());
  next();
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

#define defval(name, val)                                                      \
  void name(void) {                                                            \
    stackpush(val);                                                            \
    next();                                                                    \
  }

defval(latest, memoff(dictlatest))
defval(here, memoff(nmem))
defval(state, compiling)

#define defbinary(name, op)                                                    \
  void name(void) {                                                            \
    int x, y;                                                                  \
    if (stackpop(&y) && stackpop(&x))                                          \
      stackpush(op);                                                           \
    next();                                                                    \
  }

defbinary(add, x + y)
defbinary(sub, x - y)
defbinary(mul, x *y)
defbinary(equal, x == y)
defbinary(greaterthan, x > y)
defbinary(lessthan, x < y)
defbinary(mod, x % y)

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

int alignint(int x) {
  int mask = sizeof(int) - 1;
  assert(x < INT_MAX - mask);
  return (x + mask) & ~mask;
}

void aligned(void) {
  int x;
  if (stackpop(&x)) {
    x = alignint(x);
    stackpush(x);
  }
  next();
}

int Strdup(char *s) {
  int w = *nmem, len = strlen(s), totallen = alignint(sizeof(int) + len);
  assert(totallen <= memsize - *nmem);
  *intaddr(*nmem) = len;
  memmove(wordaddr(w), s, len);
  *nmem += totallen;
  return w;
}

void docol(void) {
  rstackpush(vm.next);
  vm.next = entryaddr(vm.current)->def;
  next();
}

struct entry *addentry(void) {
  struct entry *de;
  int size = alignint(sizeof(*de));
  *nmem = alignentry(*nmem);
  de = entryaddr(*nmem);
  assert(size <= memsize - *nmem);
  *nmem += size;
  return de;
}

void create_(void) {
  struct entry *de = addentry();
  assert(*wordbuf);
  de->word = Strdup(wordbuf);
  de->flags = 0;
  de->func = docol;
  de->def = *nmem;
  de->next = memoff(dictlatest);
  dictlatest = de;
}

void create(void) {
  create_();
  next();
}

void hidden(void) {
  int i;
  if (stackpop(&i))
    entryaddr(i)->flags ^= F_HIDDEN;
  next();
}

void hiddenset(void) {
  int i;
  if (stackpop(&i))
    entryaddr(i)->flags |= F_HIDDEN;
  next();
}

void hiddenclr(void) {
  int i;
  if (stackpop(&i))
    entryaddr(i)->flags &= ~F_HIDDEN;
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
  stackpush(*intaddr(intpp(&vm.next)));
  next();
}

void branch(void) {
  vm.next += *intaddr(vm.next);
  next();
}

void branch0(void) {
  int x;
  if (stackpop(&x) && !x) {
    branch();
  } else {
    intpp(&vm.next); /* discard jump offset */
    next();
  }
}

void comma(void) {
  stackpop(intaddr(intpp(nmem)));
  next();
}

void charcomma(void) {
  int x;
  if (stackpop(&x)) {
    assert(*nmem < memsize);
    mem[(*nmem)++] = x;
  }
  next();
}

void store(void) {
  int addr;
  if (stackpop(&addr))
    stackpop(intaddr(addr));
  next();
}

void fetch(void) {
  int addr;
  if (stackpop(&addr))
    stackpush(*intaddr(addr));
  next();
}

void interpret(void) {
  int x, ch, de;
  if (gettoken(), !*wordbuf)
    exit(0);
  if (de = find(dictlatest, wordbuf), de) {
    if (compiling && !(entryaddr(de)->flags & F_IMMEDIATE)) {
      addhere(de);
    } else {
      /* Do not call next(), jump straight to de. Someone else will call next()
       * eventually. */
      vm.current = de;
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
  stackpush(*intaddr(intpp(&vm.next)));
  next();
}

void tell(void) {
  int addr;
  if (stackpop(&addr))
    fwrite(wordaddr(addr), 1, *intaddr(addr), stdout);
  next();
}

void litstring(void) {
  int addr = intpp(&vm.next), len;
  assert(addr >= 0 && addr < memsize);
  len = mem[addr];
  assert(len >= 0 && len < memsize - vm.next);
  stackpush(addr);
  vm.next += alignint(len);
  next();
}

void dumplatest(void) {
  struct entry *de = dictlatest;
  int x = de->def, i, wrap = 16;
  for (i = 0; x < *nmem; i++, x++)
    printf("%s%02x%s", ((i % wrap) ? " " : ""), mem[x],
           ((i % wrap) == wrap - 1 ? "\n" : ""));
  putchar('\n');
  next();
}

void defword(char *word, int flags, char *def) {
  struct entry *de = addentry();
  de->word = Strdup(word);
  de->flags = flags | F_HIDDEN;
  de->def = *nmem;
  de->func = docol;
  de->next = memoff(dictlatest);
  while (*def) {
    char *p = wordbuf;
    while (space(*def))
      def++;
    while (*def && !space(*def))
      if (p < endof(wordbuf))
        *p++ = *def++;
    assert(p < endof(wordbuf));
    *p = 0;
    compile(de, wordbuf);
  }
  de->flags &= ~F_HIDDEN;
  dictlatest = de;
}

void initdict(void) {
  int i;
  char quitbuf[40];
  struct {
    char *word;
    void (*func)(void);
    int flags;
  } builtin[] = {
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
      {"<", lessthan},
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
      {"key", key},
      {"state", state},
      {"litstring", litstring},
      {"tell", tell},
      {"c,", charcomma},
      {"aligned", aligned},
      {"dumplatest", dumplatest},
      {"mod", mod},
  };
  for (i = 0; i < nelem(builtin); i++) {
    struct entry *de = addentry();
    de->word = Strdup(builtin[i].word);
    de->func = builtin[i].func;
    de->flags = builtin[i].flags;
    de->next = dictlatest ? memoff(dictlatest) : 0;
    dictlatest = de;
  }
  assert(snprintf(quitbuf, sizeof(quitbuf), "lit 0 rsp! interpret branch %d",
                  -2 * (int)sizeof(int)) < sizeof(quitbuf));
  defword("quit", 0, quitbuf);
  defword(":", 0, "word create latest hiddenset ] exit");
  defword(";", F_IMMEDIATE, "' exit , latest hiddenclr [ exit");
}

int main(void) {
  intpp(nmem); /* nmem points to mem[0]. Advance nmem so that it does not get
                  overwritten itself. */
  initdict();
  vm.current = find(dictlatest, "quit");
  while (1)
    entryaddr(vm.current)->func();
  return 0;
}
