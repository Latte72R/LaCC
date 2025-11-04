#ifndef RUNTIME_H
#define RUNTIME_H

#include <stdio.h>

#include "parser.h"

extern int show_warning;
extern int warning_cnt;
extern char *user_input;
extern CharPtrList *user_input_list;
extern Token *token;
extern Token *token_head;
extern Node **code;
extern int label_cnt;
extern int array_cnt;
extern int struct_literal_cnt;
extern int loop_id;
extern int variable_cnt;
extern int block_cnt;
extern int block_id;
extern Node *current_switch;
extern Function *functions;
extern Function *current_fn;
extern LVar *locals;
extern LVar *globals;
extern LVar *statics;
extern Object *structs;
extern Object *unions;
extern Object *enums;
extern Object *current_enum_scope;
extern TypeTag *type_tags;
extern String *strings;
extern Array *arrays;
extern StructLiteral *struct_literals;
extern FileName *filenames;
extern char *input_file;
extern char *output_file;
extern Location *consumed_loc;
extern IncludePath *include_paths;
extern FILE *fp;
extern Macro *macros;

void init_global_variables(void);

void free_user_input_list(void);
void free_all_tokens(void);
void free_all_macros(void);
void register_node(Node *node);
void free_all_nodes(void);
void register_lvar(LVar *var);
void free_all_lvars(void);
void register_type(Type *type);
void free_all_types(void);
void register_char_ptr(char *str);
void update_char_ptr(char *old_ptr, char *new_ptr);
void free_all_char_ptrs(void);
void register_location(Location *loc);
void free_all_locations(void);
void register_object(Object *object);
void free_all_objects(void);
void free_all_functions(void);
void free_all_type_tags(void);
void free_all_strings(void);
void free_all_filenames(void);
void free_all_arrays(void);
void free_all_include_paths(void);

void *safe_realloc_array(void *ptr, int elem_size, int need, int *cap);

#endif // RUNTIME_H
