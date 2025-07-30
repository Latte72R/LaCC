// extension.c
// 可変長引数を扱う関数群を定義

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

extern char *user_input;
extern char *input_file;
extern FILE *fp;

// Write formatted output to the output file.
void write_file(char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(fp, fmt, args);
  va_end(args);
}

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
  fprintf(stderr, "\033[1m\033[32m%s:%d\033[0m:\n", input_file, line_num);
  fprintf(stderr, "%.*s\n", (int)(end - line), line);

  // エラー箇所を"^"で指し示して、エラーメッセージを表示
  int pos = loc - line;
  char fmt2[128];
  sprintf(fmt2, "\033[1m\033[31m^\n%s\033[0m\n", fmt);
  fprintf(stderr, "%*s", pos, ""); // pos個の空白を出力
  vfprintf(stderr, fmt2, ap);
  exit(1);
}

// 警告の起きた場所を報告するための関数
void warning_at(char *loc, char *fmt, ...) {
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
  fprintf(stderr, "\033[1m\033[32m%s:%d\033[0m: ", input_file, line_num);
  fprintf(stderr, "%.*s\n", (int)(end - line), line);

  // エラー箇所を"^"で指し示して、エラーメッセージを表示
  int pos = loc - line;
  char fmt2[128];
  sprintf(fmt2, "\033[1m\033[33m^\n%s\033[0m\n", fmt);
  fprintf(stderr, "%*s", pos, ""); // pos個の空白を出力
  vfprintf(stderr, fmt2, ap);
}
