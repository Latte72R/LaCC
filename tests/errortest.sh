#!/bin/bash

BUILD_DIR=$1
CC=$2
mkdir -p "$BUILD_DIR"
TMP_C=$3
TMP_S=$4
TMP_OUT=$5

printf "\e[1;36mTest case 1:\e[0m\n"
cat <<EOF > "$TMP_C"
int main() {
  int x;
  int *const p1 = &x;  // ポインタ自身を変更不可
  p1 = &x;
}
EOF
cat "$TMP_C"
$CC $TMP_C
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
$CC $TMP_C
printf "\n"

printf "\e[1;36mTest case 3:\e[0m\n"
cat <<EOF > "$TMP_C"
int main() {
  int x;
  const int * p3 = &x;  // ポインタ先の値を変更不可
  *p3 = 7;
}
EOF
cat "$TMP_C"
$CC $TMP_C
printf "\n"

printf "\e[1;36mTest case 4:\e[0m\n"
cat <<EOF > "$TMP_C"
int main() {
    var = 5;  // 「未定義の識別子」エラー
}
EOF
cat "$TMP_C"
$CC $TMP_C
printf "\n"

printf "\e[1;36mTest case 5:\e[0m\n"
cat <<EOF > "$TMP_C"
int main() {
  int a[-5];  // 負のサイズは許容されない
}
EOF
cat "$TMP_C"
$CC $TMP_C
printf "\n"

printf "\e[1;36mTest case 6:\e[0m\n"
cat <<EOF > "$TMP_C"
int main() {
  int x = 5;
  *x = 3;  // dereference できない
}
EOF
cat "$TMP_C"
$CC $TMP_C
printf "\n"

printf "\e[1;36mTest case 7:\e[0m\n"
cat <<EOF > "$TMP_C"
int main() {
  /* 文字列リテラルの変更 */
  char *s = "hello";
  s[0] = 'H'; // 未定義動作 - セグフォルト
  return s[0];
}
EOF
cat "$TMP_C"
$CC $TMP_C -S -o $TMP_S
cc $TMP_S -o $TMP_OUT
$TMP_OUT
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
$CC $TMP_C -S -o $TMP_S
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
$CC $TMP_C -S -o $TMP_S
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
$CC $TMP_C -S -o $TMP_S
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
$CC $TMP_C -S -o $TMP_S
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
$CC $TMP_C -S -o $TMP_S
printf "\n"

printf "\e[1;36mTest case 13:\e[0m\n"
cat <<EOF > "$TMP_C"
int main() {
  const int arr[3] = {1, 2, 3};
  arr[2] = 10; // 定数配列の要素を変更
}
EOF
cat "$TMP_C"
$CC $TMP_C -S -o $TMP_S
printf "\n"

printf "\e[1;36mTest case 14:\e[0m\n"
cat <<EOF > "$TMP_C"
int main() {
  const int x = 5;
  x = 10;  // 定数の変更
}
EOF
cat "$TMP_C"
$CC $TMP_C -S -o $TMP_S
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
$CC $TMP_C -S -o $TMP_S
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
$CC $TMP_C -S -o $TMP_S
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
$CC $TMP_C -S -o $TMP_S
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
$CC $TMP_C -S -o $TMP_S
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
$CC $TMP_C -S -o $TMP_S
printf "\n"
