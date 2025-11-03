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
