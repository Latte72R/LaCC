

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

extern char *user_input;
extern char *filename;

// Reports an error and exit.
void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "\033[31m");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\033[0m\n");
  exit(1);
}

// エラーの起きた場所を報告するための関数
void error_at(char *loc, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  // locが含まれている行の開始地点と終了地点を取得
  char *line = loc;
  while (user_input < line && line[-1] != '\n')
    line--;

  char *end = loc;
  while (*end != '\n') {
    if (*end == '\0') {
      break;
    }
    end++;
  }

  // 見つかった行が全体の何行目なのかを調べる
  int line_num = 1;
  for (char *p = user_input; p < line; p++)
    if (*p == '\n')
      line_num++;

  // 見つかった行を、ファイル名と行番号と一緒に表示
  int indent = fprintf(stderr, "\033[1m\033[32m %s:%d\033[0m: ", filename, line_num) - 13;
  fprintf(stderr, "%.*s\n", (int)(end - line), line);

  // エラー箇所を"^"で指し示して、エラーメッセージを表示
  int pos = loc - line + indent;
  char fmt2[128];
  sprintf(fmt2, "\033[1m\033[31m^\033[0m %s\n", fmt);
  fprintf(stderr, "%*s", pos, ""); // pos個の空白を出力
  vfprintf(stderr, fmt2, ap);
  exit(1);
}

// 指定されたファイルの内容を返す
char *read_file(char *path) {
  // ファイルを開く
  FILE *fp = fopen(path, "r");
  if (!fp)
    error("cannot open %s", path);

  // ファイルの長さを調べる
  if (fseek(fp, 0, SEEK_END) == -1)
    error("%s: fseek", path);
  size_t size = ftell(fp);
  if (fseek(fp, 0, SEEK_SET) == -1)
    error("%s: fseek", path);

  // ファイル内容を読み込む
  char *buf = calloc(1, size + 2);
  fread(buf, size, 1, fp);

  // ファイルが必ず"\n\0"で終わっているようにする
  if (size == 0 || buf[size - 1] != '\n')
    buf[size++] = '\n';
  buf[size] = '\0';
  fclose(fp);
  return buf;
}
