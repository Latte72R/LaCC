
#include "lacc.h"

int show_warning = 1;
int warning_cnt = 0;
char *user_input = 0;
Token *token = 0;
Token *token_head = 0;
Node **code = 0;
int label_cnt = 0;
int array_cnt = 0;
int loop_id = -1;
int variable_cnt = 0;
int block_cnt = 0;
int block_id = 0;
Node *current_switch = 0;
Function *functions = 0;
Function *current_fn = 0;
LVar *locals = 0;
LVar *globals = 0;
LVar *statics = 0;
Object *structs = 0;
Object *unions = 0;
Object *enums = 0;
TypeTag *type_tags = 0;
String *strings = 0;
Array *arrays = 0;
String *filenames = 0;
char *input_file = 0;
char *output_file = 0;
Location *consumed_loc = 0;

IncludePath *include_paths = 0;
FILE *fp = 0;

void *NULL = 0;
const int TRUE = 1;
const int FALSE = 0;

void init_global_variables() {
  // グローバル変数の初期化
  current_switch = NULL;

  // 各リストの初期化
  functions = NULL;
  current_fn = NULL;
  enums = NULL;
  structs = NULL;
  unions = NULL;
  type_tags = NULL;
  locals = NULL;
  globals = NULL;
  statics = NULL;
  strings = NULL;
  arrays = NULL;
  token = NULL;
  token_head = NULL;
  include_paths = NULL;

  // fileポインタの初期化
  fp = NULL;
  input_file = NULL;
  output_file = NULL;
}
