
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
int optimize_level = 0;

IncludePath *include_paths = NULL;
FILE *fp = NULL;
Macro *macros = NULL;
