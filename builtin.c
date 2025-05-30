#include <stdio.h>

int main(void) {
  int c;
  printf("#ifndef BUILTIN_H\n#define BUILTIN_H\nchar builtinforth[]={");
  while (c = getchar(), c != EOF)
    printf(" %d,", c);
  printf("};\n#endif\n");
  return 0;
}
