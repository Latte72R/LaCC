
#include "lacc.h"

extern char *user_input;
extern CharPtrList *user_input_list;
extern Token *token;
extern Token *token_head;
extern FileName *filenames;
extern char *input_file;

extern const int TRUE;
extern const int FALSE;
extern void *NULL;

void handle_include_directive(char *name, char *p) {
  int already_included = FALSE;
  for (FileName *s = filenames; s; s = s->next) {
    if (!strcmp(s->name, name)) {
      // 同じファイルを二重に取り込まない
      already_included = TRUE;
      break;
    }
  }

  if (already_included) {
    free(name);
    return;
  }

  char *input_file_prev = input_file;
  input_file = name; // 現在処理中のファイル名を更新
  // filenames に現在のファイルを追加
  FileName *filename = malloc(sizeof(FileName));
  filename->name = input_file;
  filename->next = filenames;
  filenames = filename;
  char *user_input_prev = user_input;
  char *new_input = read_include_file(name); // ファイル内容を取得
  CharPtrList *user_input_list_prev = user_input_list;
  user_input_list = malloc(sizeof(CharPtrList));
  user_input_list->str = new_input;
  user_input_list->next = user_input_list_prev;
  if (!new_input) {
    error_at(new_location(p - 1), "Cannot open include file: %s", name);
  }
  // 新しいファイルをトークナイズ
  user_input = new_input;
  tokenize();
  // トークナイズ後は元の入力に戻す
  user_input = user_input_prev;
  input_file = input_file_prev;
}

// char *handle_define_directive(char *p) {
//   char *q;
//   p += 7; // "#define" の直後まで進める
//   while (isspace(*p)) {
//     p++; // 空白を読み飛ばす
//   }
//   q = p; // マクロ名の開始位置を記録
//   while (!isspace(*p) && *p != '\n' && *p != '\0') {
//     p++; // マクロ名をスキップ
//   }
//   int name_len = p - q;
//   char *name = malloc(sizeof(char) * (name_len + 1));
//   memcpy(name, q, name_len);
//   name[name_len] = '\0';
//   while (isspace(*p)) {
//     p++; // 空白を読み飛ばす
//   }
//   char *value_start = p; // マクロ値の開始位置を記録
//   while (*p != '\n' && *p != '\0') {
//     p++; // マクロ値をスキップ
//   }
//   int value_len = p - value_start;
//   char *value = malloc(sizeof(char) * (value_len + 1));
//   memcpy(value, value_start, value_len);
//   value[value_len] = '\0';
//   // マクロ定義を登録
//   define_macro(name, value);
//   free(name);
//   free(value);
//   return p;
// }

int parse_include_directive(char **p) {
  if (!(startswith(*p, "#include") && !is_alnum((*p)[8]))) {
    return 0;
  }

  char *q;
  *p += 8; // "#include" の直後まで進める
  while (isspace(**p)) {
    (*p)++; // 空白を読み飛ばす
  }
  if (**p != '"') {
    error_at(new_location(*p), "expected \" before \"include\"");
  }
  (*p)++; // 先頭の引用符をスキップ
  q = *p; // ファイル名の開始位置を記録
  while (**p != '"') {
    if (**p == '\0') {
      error_at(new_location(q - 1), "unclosed string literal [in tokenize]");
    }
    if (**p == '\\') {
      (*p) += 2; // エスケープされた文字を飛ばす
    } else {
      (*p)++;
    }
  }

  // 抽出したファイル名を確保してコピー
  char *name = malloc(sizeof(char) * ((*p) - q + 1));
  memcpy(name, q, (*p) - q);
  name[(*p) - q] = '\0';
  (*p)++; // 閉じる引用符の次へ進める
  handle_include_directive(name, q);
  return 1;
}
