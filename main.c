
#include "lcc.h"

char *user_input;
Token *token;
Node *code[100];
int labelseq = 1;
int variable_cnt = 0;
int loop_id = -1;
Function *functions;
Function *current_fn;
LVar *globals;
Struct *structs;
StructTag *struct_tags;
String *strings;
char *filename;

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

int main(int argc, char **argv) {
  if (argc != 2) {
    error("引数の個数が正しくありません");
    return 1;
  }

  // トークナイズしてパースする
  // 結果はcodeに保存される
  filename = argv[1];
  user_input = read_file(filename);
  token = tokenize();
  program();

  // アセンブリの前半部分を出力
  printf(".intel_syntax noprefix\n");
  printf(".globl main\n");

  // グローバル変数の定義
  printf(".data\n");
  for (LVar *var = globals; var; var = var->next) {
    printf("%.*s:\n", var->len, var->name);
    if (var->offset) {
      printf("  .long %d\n", var->offset);
    } else {
      printf("  .zero %d\n", get_type_size(var->type));
    }
  }

  // 文字列リテラル
  for (String *str = strings; str; str = str->next) {
    printf(".L.str%d:\n", str->label);
    printf("  .string \"%.*s\"\n", str->len, str->text);
  }

  // 先頭の式から順にコード生成
  printf(".text\n");
  for (int i = 0; code[i]; i++) {
    gen(code[i]);
  }
  return 0;
}
