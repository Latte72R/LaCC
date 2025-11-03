// Minimal reproduction of Darwin-style bitfield control structure.

#define __DARWIN_UNIX03 1
#if __DARWIN_UNIX03
#define _STRUCT_FP_CONTROL struct __darwin_fp_control
_STRUCT_FP_CONTROL {
  unsigned short __invalid : 1, __denorm : 1, __zdiv : 1, __ovrfl : 1, __undfl : 1, __precis : 1, : 2, __pc : 2, __rc : 2, : 1, : 3;
};
typedef _STRUCT_FP_CONTROL __darwin_fp_control_t;
#endif

int mac_bitfield_test1(void) {
  return (int)sizeof(__darwin_fp_control_t);
}
