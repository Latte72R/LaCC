#ifndef CODEGEN_BITSET_H
#define CODEGEN_BITSET_H

#include <string.h>

static inline int bit_words(int nbits) {
  if (nbits <= 0) return 1;
  return (nbits + 63) / 64;
}
static inline unsigned long long bit_mask(int bit) { return 1ULL << (bit & 63); }
static inline void bit_set(unsigned long long *bits, int bit) { bits[bit / 64] |= bit_mask(bit); }
static inline int bit_get(const unsigned long long *bits, int bit) { return (bits[bit / 64] & bit_mask(bit)) != 0; }
static inline unsigned long long *bit_row(unsigned long long *rows, int row, int words) { return rows + ((size_t)row * words); }
static inline const unsigned long long *cbit_row(const unsigned long long *rows, int row, int words) { return rows + ((size_t)row * words); }
static inline void bit_or(unsigned long long *dst, const unsigned long long *src, int words) {
  for (int i = 0; i < words; i++) dst[i] |= src[i];
}
static inline void bit_copy(unsigned long long *dst, const unsigned long long *src, int words) {
  memcpy(dst, src, sizeof(unsigned long long) * (size_t)words);
}

#endif
