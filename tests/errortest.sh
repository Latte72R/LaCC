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
