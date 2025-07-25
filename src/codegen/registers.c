
#include "lacc.h"

extern void *NULL;

char *regs1(int i) {
  if (i == 0)
    return "dil";
  else if (i == 1)
    return "sil";
  else if (i == 2)
    return "dl";
  else if (i == 3)
    return "cl";
  else if (i == 4)
    return "r8b";
  else if (i == 5)
    return "r9b";
  else
    return NULL;
}

char *regs4(int i) {
  if (i == 0)
    return "edi";
  else if (i == 1)
    return "esi";
  else if (i == 2)
    return "edx";
  else if (i == 3)
    return "ecx";
  else if (i == 4)
    return "r8d";
  else if (i == 5)
    return "r9d";
  else
    return NULL;
}

char *regs8(int i) {
  if (i == 0)
    return "rdi";
  else if (i == 1)
    return "rsi";
  else if (i == 2)
    return "rdx";
  else if (i == 3)
    return "rcx";
  else if (i == 4)
    return "r8";
  else if (i == 5)
    return "r9";
  else
    return NULL;
}
