
#include "lacc.h"

char *user_input;
Token *token;
Node **code;
int label_cnt = 0;
int array_cnt = 0;
int loop_cnt = 0;
int loop_id = -1;
int variable_cnt = 0;
int logical_cnt = 0;
int block_cnt = 0;
int block_id = 0;
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
char *input_file;
char *output_file;
char *consumed_ptr;

IncludePath *include_paths;
FILE *fp;

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
  include_paths->next = NULL;

  // fileポインタの初期化
  fp = NULL;
  input_file = NULL;
  output_file = NULL;
}
