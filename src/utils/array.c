#include "runtime.h"

#include "diagnostics.h"

#include <stdlib.h>

void *safe_realloc_array(void *ptr, int elem_size, int need, int *cap) {
  int old_cap = *cap;
  *cap = (*cap > 0) ? *cap : 8;
  while (need > *cap) {
    *cap *= 2;
  }
  if (old_cap != *cap) {
    void *new_ptr = realloc(ptr, elem_size * *cap);
    if (!new_ptr)
      error("realloc failed");
    ptr = new_ptr;
  }
  return ptr;
}
