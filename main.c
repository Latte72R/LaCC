
#include "lacc.h"

char *user_input;
Token *token;
Node **code;
int label_cnt = 0;
int array_cnt = 0;
int loop_cnt = 0;
int variable_cnt = 0;
int logical_cnt = 0;
int loop_id = -1;
Function *functions;
Function *current_fn;
LVar *globals;
Struct *structs;
StructTag *struct_tags;
Enum *enums;
LVar *enum_members;
String *strings;
Array *arrays;
String *filenames;
char *filename;
char *consumed_ptr;

const int TRUE = 1;
const int FALSE = 0;
void *NULL = 0;

void init_global_variables() {
  // functionの初期化
  functions = malloc(sizeof(Function));
  functions->next = NULL;
  current_fn = functions;

  // enumの初期化
  enums = malloc(sizeof(Enum));
  enums->next = NULL;
  enum_members = malloc(sizeof(LVar));
  enum_members->next = NULL;
  enum_members->type = new_type(TY_NONE);

  // 構造体の初期化
  structs = malloc(sizeof(Struct));
  structs->next = NULL;
  struct_tags = malloc(sizeof(StructTag));
  struct_tags->next = NULL;

  // グローバル変数の初期化
  globals = malloc(sizeof(LVar));
  globals->next = NULL;
  globals->type = new_type(TY_NONE);

  // 文字列リテラルの初期化
  strings = malloc(sizeof(String));
  strings->next = NULL;
  arrays = malloc(sizeof(Array));
  arrays->next = NULL;

  // トークンの初期化
  token = malloc(sizeof(Token));
  token->next = NULL;
  token->str = NULL;
  token->len = 0;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    error("引数の個数が正しくありません");
    return 1;
  }

  init_global_variables();

  // トークナイズしてパースする
  // 結果はcodeに保存される
  Token *token_cpy = token;
  filename = argv[1];
  filenames = malloc(sizeof(String));
  filenames->text = filename;
  filenames->len = strlen(filename);
  filenames->next = NULL;
  user_input = read_file(filename);

  tokenize();
  new_token(TK_EOF, NULL, 0);
  token = token_cpy->next;

  program();

  // アセンブリの前半部分を出力
  printf(".intel_syntax noprefix\n");

  // グローバル変数の定義
  printf("  .data\n");
  for (LVar *var = globals; var->next; var = var->next) {
    if (var->ext) {
      continue;
    }
    printf("  .globl %.*s\n", var->len, var->name);
    printf("  .p2align 3\n");
    printf("%.*s:\n", var->len, var->name);
    if (var->offset) {
      printf("  .long %d\n", var->offset);
    } else {
      printf("  .zero %d\n", get_sizeof(var->type));
    }
  }

  // 文字列リテラル
  for (String *str = strings; str->next; str = str->next) {
    printf(".L.str%d:\n", str->id);
    printf("  .string \"%.*s\"\n", str->len, str->text);
  }

  // 配列リテラル
  for (Array *arr = arrays; arr->next; arr = arr->next) {
    printf(".L.arr%d:\n", arr->id);
    for (int i = 0; i < arr->len; i++) {
      if (arr->byte == 1) {
        printf("  .byte %d\n", arr->val[i]);
      } else if (arr->byte == 4) {
        printf("  .long %d\n", arr->val[i]);
      } else {
        error("invalid array type [INIT]");
      }
    }
  }

  // 先頭の式から順にコード生成
  printf("  .text\n");
  for (int i = 0; code[i]->kind != ND_NONE; i++) {
    gen(code[i]);
  }
  return 0;
}
