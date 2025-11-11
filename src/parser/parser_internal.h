#ifndef PARSER_INTERNAL_H
#define PARSER_INTERNAL_H

#include "parser.h"

void error_duplicate_name(Token *tok, const char *type);
int peek(char *op);
int consume(char *op);
Token *consume_ident();
Token *expect_ident(char *stmt);
void expect(char *op, char *err, char *st);
int expect_number(char *stmt);
int parse_sign();
int expect_signed_number();
String *string_literal();
Array *array_literal(Type *org_type);
StructLiteral *struct_literal(Type *type);
Node *expr();

Node *function_definition(Token *tok, Type *type, int is_static, int is_inline);
Node *local_variable_declaration(Token *tok, Type *type, int is_static);
Node *global_variable_declaration(Token *tok, Type *type, int is_static);
Node *extern_variable_declaration(Token *tok, Type *type);
Node *vardec_and_funcdef_stmt(int is_static, int is_extern, int is_inline);
Object *struct_and_union_declaration(int is_struct, int is_union, int should_record);
Object *enum_declaration(int should_record);
Node *typedef_stmt();
Node *handle_array_initialization(Node *node, LVar *lvar, Type *type, int set_offset);
Node *handle_scalar_initialization(Node *node, Type *type, Location *loc);
Node *handle_variable_initialization(Node *node, LVar *lvar, Type *type, int set_offset);

Node *block_stmt();
Node *goto_stmt();
Node *label_stmt();
Node *if_stmt();
Node *while_stmt();
Node *do_while_stmt();
Node *for_stmt();
Node *break_stmt();
Node *continue_stmt();
Node *return_stmt();
Node *switch_stmt();
Node *case_stmt();
Node *default_stmt();
int check_label();
Node *expression_stmt();
Node *stmt();

Node *assign_sub(Node *lhs, Node *rhs, Location *loc, int check_const);
Node *assign();
Node *ternary_operator();
Node *type_cast();
Node *logical_or();
Node *logical_and();
Node *bit_or();
Node *bit_xor();
Node *bit_and();
Node *equality();
Node *relational();
Node *bit_shift();
Node *new_add(Node *lhs, Node *rhs, Location *loc);
Node *new_sub(Node *lhs, Node *rhs, Location *loc);
Node *add();
Type *resolve_type_mul(Type *left, Type *right, Location *loc);
Node *mul();
Node *unary();
Node *increment_decrement();
Node *access_member();
Node *primary();
int compile_time_number();
Node *lower_builtin_function_call(Node *call);

#endif // PARSER_INTERNAL_H
