typedef struct {
  int a;
  int b;
} ABI;

int struct_init_test1() {
  ABI p = {.a = 1, .b = 2};
  return p.a + p.b;
}

typedef struct {
  int score;
  char tag[4];
} WithArray;

WithArray struct_init_global_val = {.score = 5, .tag = "XY"};

int struct_init_test2() {
  return struct_init_global_val.score + struct_init_global_val.tag[1];
}

static WithArray struct_init_static_val = {.score = 3, .tag = "A"};

int struct_init_test3() {
  return struct_init_static_val.tag[0] + struct_init_static_val.tag[1];
}

typedef struct {
  ABI inner;
  int tail;
} WithNested;

int struct_init_test4() {
  WithNested v = {.inner = {.a = 7, .b = 8}, .tail = 3};
  return v.inner.a + v.inner.b + v.tail;
}

int struct_init_test5(int a, int b) {
  ABI v = {a, b};
  return v.a * 10 + v.b;
}

int struct_init_test6(int a, int b) {
  ABI v = {.b = b, .a = a};
  return v.a * 10 + v.b;
}

int struct_init_test7(int a, int b, int tail) {
  WithNested v = {.inner = {.a = a, .b = b}, .tail = tail};
  return v.inner.a * 100 + v.inner.b * 10 + v.tail;
}

int struct_init_test8(int a, int b) {
  WithArray v = {.score = a, .tag = {b, a + b, 0, 0}};
  return v.score * 100 + v.tag[0] * 10 + v.tag[1];
}

typedef struct {
  int values[3];
  int e;
  int f;
} WithRuntimeArray;

int struct_init_test9(int b, int c, int d, int e, int f) {
  WithRuntimeArray a = {{b, c, d}, e, f};
  return a.values[0] * 10000 + a.values[1] * 1000 + a.values[2] * 100 + a.e * 10 + a.f;
}

typedef struct {
  int values[2][2];
  int tail;
} WithRuntimeMatrix;

int struct_init_test10(int a, int b, int c, int tail) {
  WithRuntimeMatrix v = {{{a}, {b, c}}, tail};
  return v.values[0][0] * 1000 + v.values[0][1] * 100 + v.values[1][0] * 10 + v.values[1][1] + v.tail;
}

int struct_init_test11(int a, int b, int c, int tail) {
  WithRuntimeMatrix src = {{{a, b}, {c, 4}}, tail};
  WithRuntimeMatrix dst;
  dst = src;
  src.values[0][0] = 9;
  return dst.values[0][0] * 10000 + dst.values[0][1] * 1000 + dst.values[1][0] * 100 +
         dst.values[1][1] * 10 + dst.tail;
}
