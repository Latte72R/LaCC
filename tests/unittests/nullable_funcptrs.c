// Tests for Clang-style pointer attributes such as _Nullable and _Nonnull.

typedef int (* _Nullable mac_writer_t)(void *, char *, int);
typedef int (* _Nonnull mac_nonnull_cb_t)(int);
typedef int * __restrict mac_restrict_ptr_t;

extern int mac_like_funopen(const void *, int (* _Nullable)(void *, char *, int),
                            int (* _Nullable)(void *, const char *, int),
                            int (* _Null_unspecified)(void *, int));

static int sample_writer(void *ctx, char *buf, int len) {
  return len + (ctx != 0);
}

static int sample_reader(void *ctx, const char *buf, int len) {
  (void)ctx;
  return len + (buf != 0);
}

static int simple_cb(int v) {
  return v + 1;
}

int nullable_fp_test1(void) {
  mac_writer_t cb = sample_writer;
  return cb((void *)1, (char *)0, 5);
}

int nullable_fp_test2(void) {
  mac_writer_t cb = 0;
  int fallback = sample_reader((void *)0, (const char *)0, 3);
  return cb ? cb((void *)0, (char *)0, 99) : fallback + 28;
}

int nullable_fp_test3(void) {
  int value = 10;
  mac_restrict_ptr_t ptr = &value;
  *ptr += 1;
  return value;
}

int nullable_fp_test4(void) {
  mac_nonnull_cb_t fn = simple_cb;
  return fn(7);
}
