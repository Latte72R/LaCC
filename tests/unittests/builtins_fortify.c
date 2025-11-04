#include <stddef.h>
#include <string.h>

int builtin_object_size_test(void) {
  char buf[8];
  size_t sz0 = __builtin_object_size(buf, 0);
  size_t sz1 = __builtin_object_size(buf, 1);
  return (sz0 == (size_t)-1) && (sz1 == (size_t)-1);
}

int builtin_memcpy_chk_test(void) {
  char dst[5] = {0};
  const char src[] = "abc";
  char *ret = __builtin___memcpy_chk(dst, src, 4, sizeof(dst));
  return (ret == dst) && dst[0] == 'a' && dst[1] == 'b' && dst[2] == 'c' && dst[3] == '\0';
}

int builtin_memmove_chk_test(void) {
  char buf[6] = "hello";
  char *ret = __builtin___memmove_chk(buf + 1, buf, 5, sizeof(buf));
  return (ret == buf + 1) && buf[1] == 'h' && buf[2] == 'e' && buf[3] == 'l' && buf[4] == 'l';
}

int builtin_memset_chk_test(void) {
  char dst[4] = {0};
  char *ret = __builtin___memset_chk(dst, 'x', 3, sizeof(dst));
  return (ret == dst) && dst[0] == 'x' && dst[1] == 'x' && dst[2] == 'x' && dst[3] == 0;
}

int builtin_strcpy_chk_test(void) {
  char dst[8];
  const char *src = "hi";
  char *ret = __builtin___strcpy_chk(dst, src, sizeof(dst));
  return (ret == dst) && dst[0] == 'h' && dst[1] == 'i' && dst[2] == '\0';
}

int builtin_strncpy_chk_test(void) {
  char dst[8];
  const char *src = "abcd";
  char *ret = __builtin___strncpy_chk(dst, src, 3, sizeof(dst));
  return (ret == dst) && dst[0] == 'a' && dst[1] == 'b' && dst[2] == 'c';
}

int builtin_stpcpy_chk_test(void) {
  char dst[8];
  char *ret = __builtin___stpcpy_chk(dst, "ko", sizeof(dst));
  return (ret == dst + 2) && dst[0] == 'k' && dst[1] == 'o' && dst[2] == '\0';
}
