#ifndef LEXER_INTERNAL_H
#define LEXER_INTERNAL_H

int startswith(const char *p, const char *q);
int is_alnum(char c);
int get_line_number(char *pos);
const char *skip_spaces(const char *cur);

#endif // LEXER_INTERNAL_H
