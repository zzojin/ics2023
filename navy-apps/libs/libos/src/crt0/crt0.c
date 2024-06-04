#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main(int argc, char *argv[], char *envp[]);
extern char **environ;
void call_main(uintptr_t *args) {
  int argc = ((int *)args)[0];
  char **argv = (char **)((int *)args + 1);
  char **p = argv;
  // 注意判断 NULL 判断的是 *p, rather p != NULL
  while (*p != NULL)
      p++;
  ++p;
  environ = p;
  exit(main(argc, argv, environ));
  assert(0);
}
