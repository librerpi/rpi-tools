#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void fatal(const char *str, int e) {
  fprintf(stderr, "%s: %s\n", str, strerror(e));
  exit(1);
}

