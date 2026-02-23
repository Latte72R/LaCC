static inline int inline_kw_add1(int x) { return x + 1; }

static inline int inline_kw_mix(int a, int b) { return a * 2 + b; }

static inline int inline_kw_decl_then_def(int x);
static inline int inline_kw_decl_then_def(int x) { return x - 3; }

int inline_keyword_test1() { return inline_kw_add1(41); }

int inline_keyword_test2() { return inline_kw_mix(10, 5); }

int inline_keyword_test3() { return inline_kw_decl_then_def(10); }
