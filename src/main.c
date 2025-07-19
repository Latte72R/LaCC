
#include "lacc.h"

char *user_input;
Token *token;
Node **code;
int label_cnt = 0;
int array_cnt = 0;
int loop_cnt = 0;
int variable_cnt = 0;
int logical_cnt = 0;
int block_cnt = 0;
int loop_id = -1;
Function *functions;
Function *current_fn;
LVar *globals;
LVar *statics;
Struct *structs;
StructTag *struct_tags;
Enum *enums;
LVar *enum_members;
String *strings;
Array *arrays;
String *filenames;
char *filename;
char *consumed_ptr;

IncludePath *include_paths;

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

  // 静的な変数の初期化
  statics = malloc(sizeof(LVar));
  statics->next = NULL;
  statics->type = new_type(TY_NONE);

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

  // Includeパスの初期化
  include_paths = malloc(sizeof(IncludePath));
}

int main(int argc, char **argv) {
  init_global_variables();
  Token *token_cpy;
  filename = NULL;

  if (argc < 2) {
    error("Invalid number of arguments. Usage: lacc [options] <source file>");
  }

  for (int i = 1; i < argc; i++) {
    if (!memcmp(argv[i], "-I", 2) && i + 1 < argc) {
      IncludePath *ip = malloc(sizeof *ip);
      ip->path = argv[++i]; // "./include" など
      ip->next = include_paths;
      include_paths = ip;
    } else {
      if (argv[i][0] == '-') {
        error("Unknown option: %s", argv[i]);
      }
      token_cpy = token;
      filename = argv[i];
      filenames = malloc(sizeof(String));
      filenames->text = filename;
      filenames->len = strlen(filename);
      filenames->next = NULL;
    }
  }

  // トークナイズしてパースする
  // 結果はcodeに保存される
  if (!filename) {
    error("No source file specified.");
  }
  user_input = read_file(filename);

  tokenize();
  new_token(TK_EOF, NULL, 0);
  token = token_cpy->next;

  program();

  // アセンブリの前半部分を出力
  printf(".intel_syntax noprefix\n");

  printf("  .rodata\n");
  // 文字列リテラル
  for (String *str = strings; str->next; str = str->next) {
    printf(".L.str%d:\n", str->id);
    printf("  .string \"%.*s\"\n", str->len, str->text);
  }

  printf("  .data\n");
  // グローバル変数の定義
  for (LVar *var = globals; var->next; var = var->next) {
    if (var->ext) {
      continue;
    }
    if (var->is_static) {
      printf("  .local %.*s\n", var->len, var->name);
    } else {
      printf("  .globl %.*s\n", var->len, var->name);
    }
    printf("  .p2align 3\n");
    printf("%.*s:\n", var->len, var->name);
    if (var->offset) {
      printf("  .long %d\n", var->offset);
    } else {
      printf("  .zero %d\n", get_sizeof(var->type));
    }
  }

  // 静的な関数内変数の定義
  for (LVar *var = statics; var->next; var = var->next) {
    printf("  .local %.*s.%d\n", var->len, var->name, var->block);
    printf("  .p2align 3\n");
    printf("%.*s.%d:\n", var->len, var->name, var->block);
    if (var->offset) {
      printf("  .long %d\n", var->offset);
    } else {
      printf("  .zero %d\n", get_sizeof(var->type));
    }
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
