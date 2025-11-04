#ifndef PLATFORM_H
#define PLATFORM_H

// Platform detection flag (1 on Apple/Darwin, 0 otherwise)
#if defined(__APPLE__)
#define LACC_PLATFORM_APPLE 1
#define ASM_PREFIX "_"
#else
#define LACC_PLATFORM_APPLE 0
#define ASM_PREFIX ""
#endif

// Compose an assembly symbol from (len, name) pair used in this codebase
// Usage example:
//   write_file("  .globl " ASM_SYM_FMT "\n", ASM_SYM_ARGS(len, name));
#define ASM_SYM_FMT "%s%.*s"
#define ASM_SYM_ARGS(len, name) ASM_PREFIX, (len), (name)

#endif // PLATFORM_H
