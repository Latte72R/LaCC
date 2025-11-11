#ifndef LEXER_INTERNAL_H
#define LEXER_INTERNAL_H

#include "lexer.h"

int startswith(char *p, char *q);
int is_alnum(char c);
int get_line_number(char *pos);

int parse_define_directive(char **p);
int parse_include_directive(char **p);
int parse_if_directive(char **p);
int parse_ifdef_directive(char **p);
int parse_ifndef_directive(char **p);
int parse_elif_directive(char **p);
int parse_else_directive(char **p);
int parse_endif_directive(char **p);
int parse_undef_directive(char **p);
int parse_error_directive(char **p);
int parse_warning_directive(char **p);
int parse_pragma_directive(char **p);

int preprocess_is_skipping();
void preprocess_check_unterminated_ifs();

Macro *find_macro(char *name, int len);
void expand_macro(Macro *macro, char **args, int arg_count, int invocation_line);
char **parse_macro_arguments(const char **pp, Macro *macro, int *out_arg_count);
void define_macro(char *name, char *body, char **params, int param_count, int is_function, int is_variadic);
void undefine_macro(char *name, int len);
char *substitute_macro_body(Macro *macro, char **args, int arg_count);

char *duplicate_cstring(const char *src);
char *skip_trailing_spaces_and_comments(char *cur);
char *expand_expression_internal(const char *expr);
char *copy_trim_directive_expr(const char *start, const char *end);
int is_ident_char(char c);
int is_ident_start_char(char c);
int preprocess_evaluate_if_expression(char *expr_start, char *expr_end, int *result);

#endif // LEXER_INTERNAL_H
