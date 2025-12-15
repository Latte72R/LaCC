#!/bin/bash

BUILD_DIR=$1
CC=$2
mkdir -p "$BUILD_DIR"
TMP_C=$3
TMP_S=$4
TMP_OUT=$5

total_cases=0
expected_error_cases=0
missing_errors=()

run_expect_error() {
  total_cases=$((total_cases + 1))
  local case_no=$1
  shift
  "$@"
  local status=$?
  if [ $status -ne 0 ]; then
    expected_error_cases=$((expected_error_cases + 1))
  else
    missing_errors+=("$case_no")
  fi
}

printf "\e[1;36mTest case 1:\e[0m\n"
cat <<EOF > "$TMP_C"
int main() {
  int x;
  int *const p1 = &x;  // ポインタ自身を変更不可
  p1 = &x;
}
EOF
cat "$TMP_C"
run_expect_error 1 $CC $TMP_C
printf "\n"

printf "\e[1;36mTest case 2:\e[0m\n"
cat <<EOF > "$TMP_C"
int main() {
  int x;
  int const *p2 = &x;  // ポインタ先の値を変更不可
  *p2 = 7;
}
EOF
cat "$TMP_C"
run_expect_error 2 $CC $TMP_C
printf "\n"

printf "\e[1;36mTest case 3:\e[0m\n"
cat <<EOF > "$TMP_C"
int main() {
  int x;
  const int *p3 = &x;  // ポインタ先の値を変更不可
  *p3 = 7;
}
EOF
cat "$TMP_C"
run_expect_error 3 $CC $TMP_C
printf "\n"

printf "\e[1;36mTest case 4:\e[0m\n"
cat <<EOF > "$TMP_C"
int main() {
  var = 5;  // 「未定義の識別子」エラー
}
EOF
cat "$TMP_C"
run_expect_error 4 $CC $TMP_C
printf "\n"

printf "\e[1;36mTest case 5:\e[0m\n"
cat <<EOF > "$TMP_C"
int main() {
  int a[-5];  // 負のサイズは許容されない
}
EOF
cat "$TMP_C"
run_expect_error 5 $CC $TMP_C
printf "\n"

printf "\e[1;36mTest case 6:\e[0m\n"
cat <<EOF > "$TMP_C"
int main() {
  int x = 5;
  *x = 3;  // dereference できない
}
EOF
cat "$TMP_C"
run_expect_error 6 $CC $TMP_C
printf "\n"

printf "\e[1;36mTest case 7:\e[0m\n"
cat <<EOF > "$TMP_C"
typedef struct {
  int x;
} A;
int main() {
  A a;
  int b;
  a = b;
  return 0;
}
EOF
cat "$TMP_C"
run_expect_error 7 $CC $TMP_C -S -o $TMP_S
printf "\n"

printf "\e[1;36mTest case 8:\e[0m\n"
cat <<EOF > "$TMP_C"
int main() {
  /* caseが重複しているswitch文 */
  int x = 1;
  switch (x) {
    case 1:
    case 1:  // 重複している
      return 1;
    default:
      return 0;
  }
}
EOF
cat "$TMP_C"
run_expect_error 8 $CC $TMP_C -S -o $TMP_S
printf "\n"

printf "\e[1;36mTest case 9:\e[0m\n"
cat <<EOF > "$TMP_C"
int main() {
  /* switch文でdefaultが複数ある */
  int x = 1;
  switch (x) {
    case 1:
      return 1;
    default:
      return 0;
    default:  // 複数のdefault
      return 2;
  }
}
EOF
cat "$TMP_C"
run_expect_error 9 $CC $TMP_C -S -o $TMP_S
printf "\n" 

printf "\e[1;36mTest case 10:\e[0m\n"
cat <<EOF > "$TMP_C"
int test(int x) {
  return x;
}

int test(int x) {
  return x;
}

int main() {
  return test(5);
}
EOF
cat "$TMP_C"
run_expect_error 10 $CC $TMP_C -S -o $TMP_S
printf "\n"

printf "\e[1;36mTest case 11:\e[0m\n"
cat <<EOF > "$TMP_C"
int test(int x) {
  return x;
}

int main() {
  return test(5, 6);  // 引数が多すぎる
}
EOF
cat "$TMP_C"
run_expect_error 11 $CC $TMP_C -S -o $TMP_S
printf "\n"

printf "\e[1;36mTest case 12:\e[0m\n"
cat <<EOF > "$TMP_C"
int test(int x) {
  return x;
}

int main() {
  return test();  // 引数が足りない
}
EOF
cat "$TMP_C"
run_expect_error 12 $CC $TMP_C -S -o $TMP_S
printf "\n"

printf "\e[1;36mTest case 13:\e[0m\n"
cat <<EOF > "$TMP_C"
int main() {
  const int arr[3] = {1, 2, 3};
  arr[2] = 10; // 定数配列の要素を変更
}
EOF
cat "$TMP_C"
run_expect_error 13 $CC $TMP_C -S -o $TMP_S
printf "\n"

printf "\e[1;36mTest case 14:\e[0m\n"
cat <<EOF > "$TMP_C"
int main() {
  const int x = 5;
  x = 10;  // 定数の変更
}
EOF
cat "$TMP_C"
run_expect_error 14 $CC $TMP_C -S -o $TMP_S
printf "\n"

printf "\e[1;36mTest case 15:\e[0m\n"
cat <<EOF > "$TMP_C"
typedef struct ST ST;
struct ST {
  int a;
  int b;
};
int main() {
  ST st;
  int a = (int)st;  // 構造体をintにキャスト
  return a;
}
EOF
cat "$TMP_C"
run_expect_error 15 $CC $TMP_C -S -o $TMP_S
printf "\n"

printf "\e[1;36mTest case 16:\e[0m\n"
cat <<EOF > "$TMP_C"
void test() { return; }

int main() {
  int n;
  n = test();
}
EOF
cat "$TMP_C"
run_expect_error 16 $CC $TMP_C -S -o $TMP_S
printf "\n"

printf "\e[1;36mTest case 17:\e[0m\n"
cat <<EOF > "$TMP_C"
struct ST;
extern struct ST st;
int main() {
  return st.x;
}
EOF
cat "$TMP_C"
run_expect_error 17 $CC $TMP_C -S -o $TMP_S
printf "\n"

printf "\e[1;36mTest case 18:\e[0m\n"
cat <<EOF > "$TMP_C"
union U;
int main() {
  union U u;
  return u.x;
}
EOF
cat "$TMP_C"
run_expect_error 18 $CC $TMP_C -S -o $TMP_S
printf "\n"

printf "\e[1;36mTest case 19:\e[0m\n"
cat <<EOF > "$TMP_C"
typedef char *A;
typedef int A;
int main() {
  A x = 5;
  return x;
}
EOF
cat "$TMP_C"
run_expect_error 19 $CC $TMP_C -S -o $TMP_S
printf "\n"

printf "\e[1;36mTest case 20:\e[0m\n"
cat <<EOF > "$TMP_C"
int main() {
  int a = 1, b = 2, cond = 0;
  (cond ? a : b) = 15;
  return a + b;
}
EOF
cat "$TMP_C"
run_expect_error 20 $CC $TMP_C -S -o $TMP_S
printf "\n"

printf "\e[1;36mTest case 21:\e[0m\n"
cat <<EOF > "$TMP_C"
#error This is a test error from preprocessor. \
The lines below will not be compiled.
int main() {
  return 0;
}
EOF
cat "$TMP_C"
run_expect_error 21 $CC $TMP_C -S -o $TMP_S
printf "\n"

printf "\e[1;36mTest case 22:\e[0m\n"
cat <<EOF > "$TMP_C"
int main() {
  int x = 42;
  int *p;
  p = x;  // 整数からポインタへの暗黙変換はエラー
  return 0;
}
EOF
cat "$TMP_C"
run_expect_error 22 $CC $TMP_C -S -o $TMP_S
printf "\n"

printf "\e[1;36mTest case 23:\e[0m\n"
cat <<EOF > "$TMP_C"
void f() {
  return 1;  // void 関数で値を返している
}
int main() {
  f();
  return 0;
}
EOF
cat "$TMP_C"
run_expect_error 23 $CC $TMP_C -S -o $TMP_S
printf "\n"

printf "\e[1;36mTest case 24:\e[0m\n"
cat <<EOF > "$TMP_C"
int g() {
  return;  // 非void関数で値を返していない
}
int main() {
  return g();
}
EOF
cat "$TMP_C"
run_expect_error 24 $CC $TMP_C -S -o $TMP_S
printf "\n"

printf "\e[1;36mTest case 25:\e[0m\n"
cat <<EOF > "$TMP_C"
int *h() {
  return 1;  // ポインタ戻り値に整数 1 を返している
}
int main() {
  return h() != 0;
}
EOF
cat "$TMP_C"
run_expect_error 25 $CC $TMP_C -S -o $TMP_S
printf "\n"

printf "\e[1;35mSummary:\e[0m Expected errors %d / %d\n" "$expected_error_cases" "$total_cases"
if [ "${#missing_errors[@]}" -ne 0 ]; then
  printf "\e[1;31mNo error produced in test case: %s\e[0m\n" "${missing_errors[*]}"
  exit 1
fi
