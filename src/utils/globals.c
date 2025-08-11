
#include "lacc.h"

int show_warning = 1;
int warning_cnt = 0;
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
Node *current_switch;
Function *functions;
Function *current_fn;
LVar *locals;
LVar *globals;
LVar *statics;
Object *structs;
Object *unions;
Object *enums;
TypeTag *type_tags;
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
  // グローバル変数の初期化
  current_switch = NULL;

  // functionの初期化
  functions = malloc(sizeof(Function));
  functions->next = NULL;
  current_fn = functions;

  // object の初期化
  enums = malloc(sizeof(Object));
  enums->next = NULL;
  structs = malloc(sizeof(Object));
  structs->next = NULL;
  unions = malloc(sizeof(Object));
  unions->next = NULL;
  type_tags = malloc(sizeof(TypeTag));
  type_tags->next = NULL;

  // 変数の初期化
  locals = malloc(sizeof(LVar));
  locals->next = NULL;
  locals->type = new_type(TY_NONE);
  globals = malloc(sizeof(LVar));
  globals->next = NULL;
  globals->type = new_type(TY_NONE);
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
