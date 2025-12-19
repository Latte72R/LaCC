
#include "parser.h"
#include "source.h"

#include <stdbool.h>
#include <stdio.h>

bool show_warning = true;
int warning_cnt = 0;
char *user_input = 0;
CharPtrList *user_input_list = 0;
Token *token = 0;
Token *token_head = 0;
Node **code = 0;
int label_cnt = 0;
int array_cnt = 0;
int struct_literal_cnt = 0;
int loop_id = -1;
int variable_cnt = 0;
int block_cnt = 0;
int block_id = 0;
Node *current_switch = NULL;
Function *functions = NULL;
Function *current_fn = NULL;
LVar *locals = NULL;
LVar *globals = NULL;
LVar *statics = NULL;
Object *structs = NULL;
Object *unions = NULL;
Object *enums = NULL;
Object *current_enum_scope = NULL;
TypeTag *type_tags = NULL;
String *strings = NULL;
Array *arrays = NULL;
StructLiteral *struct_literals = NULL;
FileName *filenames = NULL;
char *input_file = NULL;
char *output_file = NULL;
Location *consumed_loc = NULL;
int hierarchy_level = -1;
bool print_include_files = false;

IncludePath *include_paths = NULL;
FILE *fp = NULL;
Macro *macros = NULL;

void init_global_variables() {
  // グローバル変数の初期化
  current_switch = NULL;
  user_input_list = NULL;

  // 各リストの初期化
  functions = NULL;
  current_fn = NULL;
  enums = NULL;
  current_enum_scope = NULL;
  structs = NULL;
  unions = NULL;
  type_tags = NULL;
  locals = NULL;
  globals = NULL;
  statics = NULL;
  strings = NULL;
  arrays = NULL;
  struct_literals = NULL;
  token = NULL;
  token_head = NULL;
  include_paths = NULL;
  macros = NULL;

  label_cnt = 0;
  array_cnt = 0;
  struct_literal_cnt = 0;

  // fileポインタの初期化
  fp = NULL;
  input_file = NULL;
  output_file = NULL;
}
