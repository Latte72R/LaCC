// extension.c
// 可変長引数を扱う関数群を定義

#include "diagnostics.h"
#include "runtime.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

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

  // エラーメッセージを表示
  char fmt2[128];
  sprintf(fmt2, "\033[1;38;5;9merror:\033[0m %s\n", fmt);
  vfprintf(stderr, fmt2, ap);

  // ファイルを削除してプログラムを終了
  if (fp) {
    fclose(fp);
    remove(output_file);
  }
  exit(1);
}

// エラーの起きた場所を報告するための関数
void error_at(Location *location, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  const char *line = location && location->line_start ? location->line_start : NULL;
  const char *end = location && location->line_end ? location->line_end : NULL;
  int line_num = location ? location->line : 0;
  int column = location ? location->column : 0;
  const char *path = location && location->path ? location->path : "<input>";

  // 見つかった行のファイル名と行番号を表示
  if (line_num <= 0)
    line_num = 1;
  fprintf(stderr, "\033[1;32m%s:%d:\033[0m ", path, line_num);

  // エラーメッセージを表示
  char fmt2[128];
  sprintf(fmt2, "\033[1;38;5;9merror:\033[0m %s\n", fmt);
  vfprintf(stderr, fmt2, ap);

  if (line && end) {
    // 見つかった行を表示
    fprintf(stderr, "%.*s\n", (int)(end - line), line);

    // エラー箇所を"^"で指し示す
    if (column < 0)
      column = 0;
    fprintf(stderr, "%*s\033[1;32m^\033[0m\n", column, "");
  }

  // ファイルを削除してプログラムを終了
  fclose(fp);
  remove(output_file);
  exit(1);
}

// 警告を報告するための関数
void warning(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  // 警告メッセージを表示
  char fmt2[128];
  sprintf(fmt2, "\033[1;38;5;3mwarning:\033[0m %s\n", fmt);
  vfprintf(stderr, fmt2, ap);

  warning_cnt++;
  va_end(ap);
}

// 警告の起きた場所を報告するための関数
void warning_at(Location *location, char *fmt, ...) {
  if (!show_warning) {
    return;
  }
  va_list ap;
  va_start(ap, fmt);

  const char *line = location && location->line_start ? location->line_start : NULL;
  const char *end = location && location->line_end ? location->line_end : NULL;
  int line_num = location ? location->line : 0;
  int column = location ? location->column : 0;
  const char *path = location && location->path ? location->path : "<input>";

  // 見つかった行のファイル名と行番号を表示
  if (line_num <= 0)
    line_num = 1;
  fprintf(stderr, "\033[1;32m%s:%d:\033[0m ", path, line_num);

  // 警告メッセージを表示
  char fmt2[128];
  sprintf(fmt2, "\033[1;38;5;3mwarning:\033[0m %s\n", fmt);
  vfprintf(stderr, fmt2, ap);

  if (line && end) {
    // 見つかった行を表示
    fprintf(stderr, "%.*s\n", (int)(end - line), line);

    // 警告箇所を"^"で指し示す
    if (column < 0)
      column = 0;
    fprintf(stderr, "%*s\033[1;32m^\033[0m\n", column, "");
  }

  warning_cnt++;
  va_end(ap);
}
