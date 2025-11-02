// Tests for #include_next lookup skipping to the next include directory
#include "shadow.h"

int include_next_test1() {
  return SHADOW1 + SHADOW2 + AFTER_NEXT; // 100 + 200 + 1 = 301
}
