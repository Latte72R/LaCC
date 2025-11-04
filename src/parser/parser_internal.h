#ifndef PARSER_INTERNAL_H
#define PARSER_INTERNAL_H

#include "parser.h"

void error_duplicate_name(Token *tok, const char *type);
int peek(char *op);
int consume(char *op);
Token *consume_ident(void);
Token *expect_ident(char *stmt);
void expect(char *op, char *err, char *st);
int expect_number(char *stmt);
int parse_sign(void);
int expect_signed_number(void);
String *string_literal(void);
Array *array_literal(Type *org_type);
StructLiteral *struct_literal(Type *type);
Node *expr(void);

Node *function_definition(Token *tok, Type *type, int is_static, int is_inline);
Node *local_variable_declaration(Token *tok, Type *type, int is_static);
Node *global_variable_declaration(Token *tok, Type *type, int is_static);
Node *extern_variable_declaration(Token *tok, Type *type);
Node *vardec_and_funcdef_stmt(int is_static, int is_extern, int is_inline);
Object *struct_and_union_declaration(int is_struct, int is_union, int should_record);
Object *enum_declaration(int should_record);
Node *typedef_stmt(void);
Node *handle_array_initialization(Node *node, LVar *lvar, Type *type, int set_offset);
Node *handle_scalar_initialization(Node *node, Type *type, Location *loc);
Node *handle_variable_initialization(Node *node, LVar *lvar, Type *type, int set_offset);

Node *block_stmt(void);
Node *goto_stmt(void);
Node *label_stmt(void);
Node *if_stmt(void);
Node *while_stmt(void);
Node *do_while_stmt(void);
Node *for_stmt(void);
Node *break_stmt(void);
Node *continue_stmt(void);
Node *return_stmt(void);
Node *switch_stmt(void);
Node *case_stmt(void);
Node *default_stmt(void);
int check_label(void);
Node *expression_stmt(void);
Node *stmt(void);

Node *assign_sub(Node *lhs, Node *rhs, Location *loc, int check_const);
Node *assign(void);
Node *ternary_operator(void);
Node *type_cast(void);
Node *logical_or(void);
Node *logical_and(void);
Node *bit_or(void);
Node *bit_xor(void);
Node *bit_and(void);
Node *equality(void);
Node *relational(void);
Node *bit_shift(void);
Node *new_add(Node *lhs, Node *rhs, Location *loc);
Node *new_sub(Node *lhs, Node *rhs, Location *loc);
Node *add(void);
Type *resolve_type_mul(Type *left, Type *right, Location *loc);
Node *mul(void);
Node *unary(void);
Node *increment_decrement(void);
Node *access_member(void);
Node *primary(void);
int compile_time_number(void);
Node *lower_builtin_function_call(Node *call);

#endif // PARSER_INTERNAL_H
