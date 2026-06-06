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
Node *handle_array_initialization(Node *node, LVar *lvar, Type *type, int is_static_storage);
Node *handle_scalar_initialization(Node *node, Type *type, Location *loc);
Node *handle_variable_initialization(Node *node, LVar *lvar, Type *type, int is_static_storage);

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

// Symbol helpers
Node *new_node(NodeKind kind);
Node *new_num(int val);
Node *new_deref(Node *lhs);
Node *new_binary(NodeKind kind, Node *lhs, Node *rhs);
LVar *new_lvar(Token *tok, Type *type, int is_static, int is_extern);
LVar *find_lvar(Token *tok);
LVar *find_gvar(Token *tok);
Object *find_struct(Token *tok);
Object *find_union(Token *tok);
Object *find_enum(Token *tok);
LVar *find_enum_member(Token *tok);
LVar *find_object_member(Object *object, Token *tok);
TypeTag *find_type_tag(Token *tok);
Function *find_fn(Token *tok);

// Type system helpers
Type *parse_base_type_internal(int should_consume, int should_record);
Type *peek_base_type();
Type *parse_pointer_qualifiers(Type *base_type);
Type *parse_declarator(Type *base_type, Token **tok, char *stmt);
Type *parse_declarator_suffix(Type *type, char *stmt);
Type *parse_function_suffix(Type *type, char *stmt);
Type *consume_type(int should_record);
Type *consume_type_name(int should_record, char *stmt);
Type *new_type(TypeKind ty);
Type *new_type_ptr(Type *ptr_to);
Type *new_type_arr(Type *ptr_to, int array_size);
Type *parse_array_dimensions(Type *base_type);
void substitute_type(Type *where, Type *placeholder, Type *actual);
int is_type(Token *tok);
int is_enum_type(Type *type);
char *type_name(Type *type);
int is_type_compatible(Type *lhs, Type *rhs);
int is_type_identical(Type *lhs, Type *rhs);
int is_type_assignable(Type *lhs, Type *rhs);
int eval_const_expr(Node *node, int *ok);

#endif // PARSER_INTERNAL_H
