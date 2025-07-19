#!/bin/bash

BUILD_DIR=$1
CC=$2
OUTPUT="$BUILD_DIR/tmp.c"
LACC_FLAGS="-I include"
mkdir -p "$BUILD_DIR"

printf "\e[1;36mTest case 1:\e[0m\n"
cat <<EOF > "$OUTPUT"
int main() {
  int x;
  int *const p1 = &x;  // ポインタ自身を変更不可
  p1 = &x;
}
EOF
cat "$OUTPUT"
$CC $LACC_FLAGS $OUTPUT
printf "\n"

printf "\e[1;36mTest case 2:\e[0m\n"
cat <<EOF > "$OUTPUT"
int main() {
  int x;
  int const *p2 = &x;  // ポインタ先の値を変更不可
  *p2 = 7;
}
EOF
cat "$OUTPUT"
$CC $LACC_FLAGS $OUTPUT
printf "\n"

printf "\e[1;36mTest case 3:\e[0m\n"
cat <<EOF > "$OUTPUT"
int main() {
  int x;
  const int * p3 = &x;  // ポインタ先の値を変更不可
  *p3 = 7;
}
EOF
cat "$OUTPUT"
$CC $LACC_FLAGS $OUTPUT
printf "\n"

printf "\e[1;36mTest case 4:\e[0m\n"
cat <<EOF > "$OUTPUT"
int main() {
    var = 5;  // 「未定義の識別子」エラー
}
EOF
cat "$OUTPUT"
$CC $LACC_FLAGS $OUTPUT
printf "\n"

printf "\e[1;36mTest case 5:\e[0m\n"
cat <<EOF > "$OUTPUT"
int main() {
  int a[-5];  // 負のサイズは許容されない
}
EOF
cat "$OUTPUT"
$CC $LACC_FLAGS $OUTPUT
printf "\n"

printf "\e[1;36mTest case 6:\e[0m\n"
cat <<EOF > "$OUTPUT"
int main() {
  int x = 5;
  *x = 3;  // dereference できない
}
EOF
cat "$OUTPUT"
$CC $LACC_FLAGS $OUTPUT
printf "\n"
