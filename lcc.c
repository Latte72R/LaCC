
//
// Tokenizer
//

typedef enum {
  TK_EOF,      // 入力の終わりを表すトークン
  TK_RESERVED, // 記号
  TK_IDENT,    // 識別子
  TK_TYPE,     // 型
  TK_NUM,      // 整数トークン
  TK_RETURN,   // return
  TK_IF,
  TK_WHILE,
  TK_FOR,
  TK_BREAK,
  TK_CONTINUE,
  TK_DO,
  TK_EXTERN,
  TK_STR,     // 文字列
  TK_TYPEDEF, // typedef
  TK_ENUM,    // enum
  TK_STRUCT   // struct
} TokenKind;

// Token type
typedef struct Token Token;
struct Token {
  TokenKind kind; // Token kind
  Token *next;    // Next token
  int val;        // If kind is TK_NUM, its value
  char *str;      // Token string
  int len;        // Token length
};

// ローカル変数の型

typedef struct String String;
struct String {
  String *next;
  char *text;
  int len;
  int label;
};

typedef enum { TY_NONE, TY_INT, TY_CHAR, TY_PTR, TY_ARR, TY_VOID, TY_STRUCT } TypeKind;

typedef struct Type Type;

typedef struct LVar LVar;
struct LVar {
  LVar *next; // 次の変数かNULL
  char *name; // 変数の名前
  int len;    // 名前の長さ
  int offset; // RBPからのオフセット
  Type *type; // 変数の型
};

typedef struct Struct Struct;
struct Struct {
  Struct *next; // 次の構造体かNULL
  LVar *var;    // 次の変数かNULL
  char *name;   // 変数の名前
  int len;      // 名前の長さ
  int size;     // 構造体のサイズ
};

typedef struct Enum Enum;
struct Enum {
  Enum *next; // 次のEnumかNULL
  char *name; // 変数の名前
  int len;    // 名前の長さ
};

typedef struct StructTag StructTag;
struct StructTag {
  StructTag *next; // 次の構造体かNULL
  Struct *main;    // struct
  char *name;      // タグの名前
  int len;         // 名前の長さ
};

struct Type {
  TypeKind ty;
  Type *ptr_to;
  int array_size;
  Struct *struct_;
};

// 関数の型
typedef struct Function Function;
struct Function {
  Function *next; // 次の関数かNULL
  LVar *locals;   // ローカル変数
  char *name;     // 変数の名前
  int len;        // 名前の長さ
  int offset;     // RBPからのオフセット
  Type *type;     // 関数の型
};

//
// Parser
//

typedef enum {
  ND_NONE,     // 空のノード
  ND_ADD,      // +
  ND_SUB,      // -
  ND_MUL,      // *
  ND_DIV,      // /
  ND_MOD,      // %
  ND_EQ,       // ==
  ND_NE,       // !=
  ND_LT,       // <
  ND_LE,       // <=
  ND_AND,      // &&
  ND_OR,       // ||
  ND_NOT,      // !
  ND_BITNOT,   // ~
  ND_BITAND,   // &
  ND_BITOR,    // |
  ND_BITXOR,   // ^
  ND_SHL,      // <<
  ND_SHR,      // >>
  ND_ASSIGN,   // =
  ND_POSTINC,  // ++ or --
  ND_LVAR,     // ローカル変数
  ND_VARDEC,   // 変数宣言
  ND_GVAR,     // グローバル変数
  ND_GLBDEC,   // グローバル変数宣言
  ND_NUM,      // 整数
  ND_STR,      // 文字列
  ND_ADDR,     // &
  ND_DEREF,    // *
  ND_IF,       // if
  ND_ELSE,     // else
  ND_WHILE,    // while
  ND_FOR,      // for
  ND_BREAK,    // break
  ND_CONTINUE, // continue
  ND_DOWHILE,  // do-while
  ND_RETURN,   // return
  ND_FUNCDEF,  // 関数定義
  ND_FUNCALL,  // 関数呼び出し
  ND_EXTERN,   // extern
  ND_BLOCK,    // { ... }
  ND_ARR,      // 配列
  ND_ENUM,     // 列挙体
  ND_STRUCT,   // 構造体
  ND_TYPEDEF,  // typedef
  ND_TYPE      // 型
} NodeKind;

// 抽象構文木のノード
typedef struct Node Node;
struct Node {
  NodeKind kind; // ノードの型
  Node *lhs;     // 左辺
  Node *rhs;     // 右辺
  int val;       // kindがND_NUMの場合はその数値
  int id;        // kindがND_IF, ND_WHILE, ND_FORの場合のみ使う
  int endline;
  Node *cond;    // kindがND_IF, ND_WHILE, ND_FORの場合のみ使う
  Node *then;    // kindがND_IFの場合のみ使う
  Node *els;     // kindがND_IFの場合のみ使う
  Node *init;    // kindがND_FORの場合のみ使う
  Node *step;    // kindがND_FORの場合のみ使う
  Node **body;   // kindがND_BLOCKの場合のみ使う
  Node *args[6]; // kindがND_FUNCALLの場合のみ使う
  Function *fn;  // kindがND_FUNCDEF, ND_FUNCALLの場合のみ使う
  LVar *var;     // kindがND_LVAR, ND_GVARの場合のみ使う
  String *str;   // kindがND_STRの場合のみ使う
  Type *type;
};

Node *stmt();
Node *assign();
Node *expr();
Node *equality();
Node *relational();
Node *new_add(Node *lhs, Node *rhs, char *consumed_ptr);
Node *new_sub(Node *lhs, Node *rhs, char *consumed_ptr);
Node *add();
Node *mul();
Node *logical_and();
Node *logical_or();
Node *bit_operator();
Node *bit_shift();
Node *increment_decrement();
Node *access_member();
Node *unary();
Node *primary();

void program();

void gen(Node *node);

// extention.c
void error();
void error_at();
char *read_file();
void init();

// stdio.h
void printf();

// ctype.h
int isspace();
int isdigit();

// string.h
int memcmp();
int strlen();
int strtol();
char *strstr();
char *strchr();

// stdlib.h
void *malloc();
void *realloc();

char *user_input;
Token *token;
Node **code;
int label_cnt = 0;
int loop_cnt = 0;
int variable_cnt = 0;
int loop_id = -1;
Function *functions;
Function *current_fn;
LVar *globals;
Struct *structs;
StructTag *struct_tags;
Enum *enums;
LVar *enum_members;
String *strings;
char *filename;
char *consumed_ptr;

int TRUE = 1;
int FALSE = 0;
void *NULL = 0;

// Create a new token and add it as the next token of `cur`.
Token *new_token(TokenKind kind, Token *cur, char *str, int len) {
  Token *tok = malloc(sizeof(Token));
  tok->kind = kind;
  tok->str = str;
  tok->len = len;
  cur->next = tok;
  return tok;
}

int startswith(char *p, char *q) { return memcmp(p, q, strlen(q)) == 0; }

int is_alnum(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || (c == '_');
}

// Tokenize `user_input` and returns new tokens.
Token *tokenize() {
  char *p = user_input;
  char *q;
  Token head;
  head.next = NULL;
  Token *cur = &head;

  while (*p) {
    // Skip whitespace characters.
    if (isspace(*p)) {
      p++;
      continue;
    }

    // 行コメントをスキップ
    if (startswith(p, "//")) {
      p += 2;
      while (*p != '\n' && *p != '\0')
        p++;
      continue;
    }

    // ブロックコメントをスキップ
    if (startswith(p, "/*")) {
      q = strstr(p + 2, "*/");
      if (!q)
        error_at(p, "unclosed block comment [in tokenize]");
      p = q + 2;
      continue;
    }

    // Multi-letter punctuator
    if (startswith(p, "==") || startswith(p, "!=") || startswith(p, "<=") || startswith(p, ">=") ||
        startswith(p, "&&") || startswith(p, "||") || startswith(p, "++") || startswith(p, "--") ||
        startswith(p, "+=") || startswith(p, "-=") || startswith(p, "*=") || startswith(p, "/=") ||
        startswith(p, "%=") || startswith(p, "->") || startswith(p, "<<") || startswith(p, ">>")) {
      cur = new_token(TK_RESERVED, cur, p, 2);
      p += 2;
      continue;
    }

    // Single-letter punctuator
    if (strchr("+-*/()<>={}[];&|^~,%!.:", *p)) {
      cur = new_token(TK_RESERVED, cur, p++, 1);
      continue;
    }

    // char
    if (*p == '\'') {
      q = p;
      int len;
      char val;
      p++;

      if (*p == '\\') {
        p++;
        if (*p == 'n') {
          val = '\n';
        } else if (*p == 't') {
          val = '\t';
        } else if (*p == 'r') {
          val = '\r';
        } else if (*p == 'b') {
          val = '\b';
        } else if (*p == 'f') {
          val = '\f';
        } else if (*p == 'v') {
          val = '\v';
        } else if (*p == '\\') {
          val = '\\';
        } else if (*p == '\'') {
          val = '\'';
        } else if (*p == '\"') {
          val = '\"';
        } else if (*p == '0') {
          val = '\0';
        } else {
          error_at(p - 1, "invalid escape sequence in character literal");
        }
        p++;
        len = 4;
      } else {
        val = *p;
        p++;
        len = 3;
      }

      if (*p != '\'') {
        error_at(p, "unclosed character literal");
      }
      p++;

      cur = new_token(TK_NUM, cur, q, len);
      cur->val = val;
      continue;
    }

    if (*p == '"') {
      p++;
      q = p;
      while (*p != '"') {
        if (*p == '\0') {
          error_at(q - 1, "unclosed string literal [in tokenize]");
        }
        if (*p == '\\') {
          p += 2;
        } else {
          p++;
        }
      }
      cur = new_token(TK_STR, cur, q, p - q);
      p++;
      continue;
    }

    // Integer literal
    if (isdigit(*p)) {
      cur = new_token(TK_NUM, cur, p, 0);
      q = p;
      cur->val = strtol(p, &p, 10);
      cur->len = p - q;
      continue;
    }

    if (startswith(p, "do") && !is_alnum(p[2])) {
      cur = new_token(TK_DO, cur, p, 2);
      p += 2;
      continue;
    }

    if (startswith(p, "sizeof") && !is_alnum(p[6])) {
      cur = new_token(TK_RESERVED, cur, p, 6);
      p += 6;
      continue;
    }

    if (startswith(p, "else") && !is_alnum(p[4])) {
      cur = new_token(TK_RESERVED, cur, p, 4);
      p += 4;
      continue;
    }

    if (startswith(p, "return") && !is_alnum(p[6])) {
      cur = new_token(TK_RETURN, cur, p, 6);
      p += 6;
      continue;
    }

    if (startswith(p, "extern") && !is_alnum(p[6])) {
      cur = new_token(TK_EXTERN, cur, p, 6);
      p += 6;
      continue;
    }

    if (startswith(p, "enum") && !is_alnum(p[4])) {
      cur = new_token(TK_ENUM, cur, p, 4);
      p += 4;
      continue;
    }

    if (startswith(p, "struct") && !is_alnum(p[6])) {
      cur = new_token(TK_STRUCT, cur, p, 6);
      p += 6;
      continue;
    }

    if (startswith(p, "typedef") && !is_alnum(p[7])) {
      cur = new_token(TK_TYPEDEF, cur, p, 7);
      p += 7;
      continue;
    }

    if (startswith(p, "if") && !is_alnum(p[2])) {
      cur = new_token(TK_IF, cur, p, 2);
      p += 2;
      continue;
    }

    if (startswith(p, "while") && !is_alnum(p[5])) {
      cur = new_token(TK_WHILE, cur, p, 5);
      p += 5;
      continue;
    }

    if (startswith(p, "for") && !is_alnum(p[3])) {
      cur = new_token(TK_FOR, cur, p, 3);
      p += 3;
      continue;
    }

    if (startswith(p, "int") && !is_alnum(p[3])) {
      cur = new_token(TK_TYPE, cur, p, 3);
      p += 3;
      continue;
    }

    if (startswith(p, "char") && !is_alnum(p[4])) {
      cur = new_token(TK_TYPE, cur, p, 4);
      p += 4;
      continue;
    }

    if (startswith(p, "void") && !is_alnum(p[4])) {
      cur = new_token(TK_TYPE, cur, p, 4);
      p += 4;
      continue;
    }

    if (startswith(p, "break") && !is_alnum(p[5])) {
      cur = new_token(TK_BREAK, cur, p, 5);
      p += 5;
      continue;
    }

    if (startswith(p, "continue") && !is_alnum(p[8])) {
      cur = new_token(TK_CONTINUE, cur, p, 8);
      p += 8;
      continue;
    }

    if (('a' <= *p && *p <= 'z') || ('A' <= *p && *p <= 'Z') || *p == '_') {
      int i = 0;
      while (('a' <= *(p + i) && *(p + i) <= 'z') || ('A' <= *(p + i) && *(p + i) <= 'Z') ||
             ('0' <= *(p + i) && *(p + i) <= '9') || *(p + i) == '_') {
        i++;
      }
      cur = new_token(TK_IDENT, cur, p, i);
      p += i;
      continue;
    }

    error_at(p, "invalid token [in tokenize]");
  }

  new_token(TK_EOF, cur, p, 0);
  return head.next;
}

// 変数を名前で検索する。見つからなかった場合はNULLを返す。
LVar *find_lvar(Token *tok) {
  for (LVar *var = current_fn->locals; var->next; var = var->next)
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// 変数を名前で検索する。見つからなかった場合はNULLを返す。
LVar *find_gver(Token *tok) {
  for (LVar *var = globals; var->next; var = var->next)
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// structを名前で検索する。見つからなかった場合はNULLを返す。
Struct *find_struct(Token *tok) {
  for (Struct *var = structs; var->next; var = var->next)
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// enumを名前で検索する。見つからなかった場合はNULLを返す。
Enum *find_enum(Token *tok) {
  for (Enum *var = enums; var->next; var = var->next)
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// enumのメンバーを名前で検索する。見つからなかった場合はNULLを返す。
LVar *find_enum_member(Token *tok) {
  for (LVar *var = enum_members; var->next; var = var->next)
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// structのメンバーを名前で検索する。見つからなかった場合はNULLを返す。
LVar *find_struct_member(Struct *struct_, Token *tok) {
  for (LVar *var = struct_->var; var->next; var = var->next)
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// struct_tagを名前で検索する。見つからなかった場合はNULLを返す。
StructTag *find_struct_tag(Token *tok) {
  for (StructTag *var = struct_tags; var->next; var = var->next)
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// 関数を名前で検索する。見つからなかった場合はNULLを返す。
Function *find_fn(Token *tok) {
  for (Function *fn = functions; fn->next; fn = fn->next)
    if (fn->len == tok->len && !memcmp(tok->str, fn->name, fn->len))
      return fn;
  return NULL;
}

// Consumes the current token if it matches `op`.
int consume(char *op) {
  if (token->kind != TK_RESERVED || strlen(op) != token->len || memcmp(token->str, op, token->len))
    return FALSE;
  consumed_ptr = token->str;
  token = token->next;
  return TRUE;
}

Token *consume_ident() {
  if (token->kind != TK_IDENT)
    return NULL;
  Token *tok = token;
  consumed_ptr = token->str;
  token = token->next;
  return tok;
}

Type *check_type() {
  Token *tok = token;
  Type *type = malloc(sizeof(Type));
  if (tok->kind == TK_IDENT) {
    Struct *struct_ = find_struct(tok);
    Enum *enum_ = find_enum(tok);
    if (struct_) {
      type->ty = TY_STRUCT;
      type->struct_ = struct_;
    } else if (enum_) {
      type->ty = TY_INT;
    } else {
      return NULL;
    }
  } else if (tok->kind != TK_TYPE) {
    return NULL;
  } else if (memcmp(tok->str, "int", tok->len) == 0) {
    type->ty = TY_INT;
  } else if (memcmp(tok->str, "char", tok->len) == 0) {
    type->ty = TY_CHAR;
  } else if (memcmp(tok->str, "void", tok->len) == 0) {
    type->ty = TY_VOID;
  } else {
    error_at(tok->str, "unknown type \"%.*s\" [in consume type]", tok->len, tok->str);
  }
  token = tok;
  return type;
}

Type *consume_type() {
  Token *tok = token;
  Type *type = malloc(sizeof(Type));
  if (tok->kind == TK_IDENT) {
    Struct *struct_ = find_struct(tok);
    Enum *enum_ = find_enum(tok);
    if (struct_) {
      type->ty = TY_STRUCT;
      type->struct_ = struct_;
    } else if (enum_) {
      type->ty = TY_INT;
    } else {
      return NULL;
    }
  } else if (tok->kind != TK_TYPE) {
    return NULL;
  } else if (memcmp(tok->str, "int", tok->len) == 0) {
    type->ty = TY_INT;
  } else if (memcmp(tok->str, "char", tok->len) == 0) {
    type->ty = TY_CHAR;
  } else if (memcmp(tok->str, "void", tok->len) == 0) {
    type->ty = TY_VOID;
  } else {
    error_at(tok->str, "unknown type \"%.*s\" [in consume type]", tok->len, tok->str);
  }
  consumed_ptr = token->str;
  token = token->next;
  while (consume("*")) {
    Type *ptr = malloc(sizeof(Type));
    ptr->ty = TY_PTR;
    ptr->ptr_to = type;
    type = ptr;
  }
  return type;
}

int is_type(Token *tok) {
  if (tok->kind == TK_TYPE)
    return TRUE;
  if (tok->kind == TK_IDENT) {
    Struct *struct_ = find_struct(tok);
    if (struct_)
      return TRUE;
    Enum *enum_ = find_enum(tok);
    if (enum_)
      return TRUE;
  }
  return FALSE;
}

// Ensure that the current token is `op`.
void expect(char *op, char *err, char *st) {
  if (token->kind != TK_RESERVED || strlen(op) != token->len || memcmp(token->str, op, token->len))
    error_at(token->str, "expected \"%s\":\n  %s  [in %s statement]", op, err, st);
  token = token->next;
}

// Ensure that the current token is TK_NUM.
int expect_number() {
  if (token->kind != TK_NUM)
    error_at(token->str, "expected a number but got \"%.*s\" [in expect_number]", token->len, token->str);
  int val = token->val;
  token = token->next;
  return val;
}

int is_ptr_or_arr(Type *type) { return type->ty == TY_PTR || type->ty == TY_ARR; }
int is_number(Type *type) { return type->ty == TY_INT || type->ty == TY_CHAR; }

// 変数として扱うときのサイズ
int get_type_size(Type *type) {
  if (type->ty == TY_INT) {
    return 4;
  } else if (type->ty == TY_CHAR) {
    return 1;
  } else if (is_ptr_or_arr(type)) {
    return 8;
  } else {
    error_at(token->str, "invalid type (%d) [in get_type_size]", type->ty);
    return 0;
  }
}

// 予約しているスタック領域のサイズ
int get_sizeof(Type *type) {
  if (type->ty == TY_INT) {
    return 4;
  } else if (type->ty == TY_CHAR) {
    return 1;
  } else if (type->ty == TY_PTR) {
    return 8;
  } else if (type->ty == TY_ARR) {
    return get_sizeof(type->ptr_to) * type->array_size;
  } else if (type->ty == TY_STRUCT) {
    return type->struct_->size;
  } else {
    error_at(token->str, "invalid type [in get_sizeof]");
    return 0;
  }
}

Type *new_type(TypeKind ty) {
  Type *type = malloc(sizeof(Type));
  type->ty = ty;
  return type;
}

Type *new_type_ptr(Type *ptr_to) {
  Type *type = malloc(sizeof(Type));
  type->ty = TY_PTR;
  type->ptr_to = ptr_to;
  return type;
}

Type *new_type_arr(Type *ptr_to, int array_size) {
  Type *type = malloc(sizeof(Type));
  type->ty = TY_ARR;
  type->ptr_to = ptr_to;
  type->array_size = array_size;
  return type;
}

Type *new_type_struct(Struct *struct_) {
  Type *type = malloc(sizeof(Type));
  type->ty = TY_STRUCT;
  type->struct_ = struct_;
  return type;
}

Node *new_node(NodeKind kind) {
  Node *node = malloc(sizeof(Node));
  node->kind = kind;
  node->endline = FALSE;
  return node;
}

Node *new_num(int val) {
  Node *node = new_node(ND_NUM);
  node->val = val;
  node->type = new_type(TY_INT);
  return node;
}

Node *new_deref(Node *lhs) {
  Node *node = new_node(ND_DEREF);
  node->lhs = lhs;
  node->type = lhs->type->ptr_to;
  return node;
}

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = new_node(kind);
  node->lhs = lhs;
  node->rhs = rhs;
  node->type = lhs->type;
  return node;
}

Node *function_definition(Token *tok, Type *type) {
  Function *fn = find_fn(tok);
  if (fn) {
    // error_at(token->str, "duplicated function name: %.*s", tok->len, tok->str);
  } else {
    fn = malloc(sizeof(Function));
    fn->next = functions;
    functions = fn;
  }
  fn->name = tok->str;
  fn->len = tok->len;
  fn->locals = malloc(sizeof(LVar));
  fn->locals->offset = 0;
  fn->locals->type = new_type(TY_NONE);
  fn->locals->next = NULL;
  fn->type = type;
  fn->offset = 0;
  Function *prev_fn = current_fn;
  current_fn = fn;
  Node *node = new_node(ND_FUNCDEF);
  node->fn = fn;
  if (!consume(")")) {
    int n = 0;
    for (int i = 0; i < 6; i++) {
      type = consume_type();
      Token *tok_lvar = consume_ident();
      if (!tok_lvar) {
        error_at(consumed_ptr, "expected an identifier [in variable declaration]");
      }
      Node *nd_lvar = new_node(ND_LVAR);
      node->args[i] = nd_lvar;
      LVar *lvar = malloc(sizeof(LVar));
      lvar->next = fn->locals;
      lvar->name = tok_lvar->str;
      lvar->len = tok_lvar->len;
      lvar->offset = fn->locals->offset + get_sizeof(type);
      fn->offset = lvar->offset;
      lvar->type = type;
      nd_lvar->var = lvar;
      nd_lvar->type = type;
      fn->locals = lvar;
      n += 1;
      if (!consume(","))
        break;
    }
    node->val = n;
    expect(")", "after arguments", "function definition");
  } else {
    node->val = 0;
  }
  if (!(token->kind == TK_RESERVED && !memcmp(token->str, "{", token->len))) {
    node->kind = ND_EXTERN;
    expect(";", "after line", "function definition");
  } else {
    node->lhs = stmt();
  }
  current_fn = prev_fn;
  return node;
}

Node *local_variable_declaration(Token *tok, Type *type) {
  LVar *lvar = find_lvar(tok);
  if (lvar) {
    error_at(tok->str, "duplicated variable name: %.*s [in variable declaration]", tok->len, tok->str);
  }
  Node *node = new_node(ND_VARDEC);
  lvar = malloc(sizeof(LVar));
  lvar->name = tok->str;
  lvar->len = tok->len;
  while (consume("[")) {
    type = new_type_arr(type, expect_number());
    expect("]", "after number", "array declaration");
  }
  lvar->offset = current_fn->locals->offset + get_sizeof(type);
  if (current_fn->offset < lvar->offset) {
    current_fn->offset = lvar->offset;
  }
  node->var = lvar;
  node->type = type;
  lvar->type = type;
  lvar->next = current_fn->locals;
  current_fn->locals = lvar;
  if (consume("=")) {
    node = new_binary(ND_ASSIGN, node, expr());
  }
  node->endline = TRUE;
  return node;
}

Node *global_variable_declaration(Token *tok, Type *type) {
  LVar *lvar = find_gver(tok);
  if (lvar) {
    error_at(token->str, "duplicated variable name: %.*s [in global variable declaration]", tok->len, tok->str);
  }
  Node *node = new_node(ND_GLBDEC);
  lvar = malloc(sizeof(LVar));
  lvar->name = tok->str;
  lvar->len = tok->len;
  while (consume("[")) {
    type = new_type_arr(type, expect_number());
    expect("]", "after number", "array declaration");
  }
  node->var = lvar;
  node->type = type;
  lvar->type = type;
  lvar->next = globals;
  globals = lvar;
  if (consume("=")) {
    int sign = 1;
    if (consume("-")) {
      sign = -1;
    } else if (consume("+")) {
      sign = 1;
    }
    lvar->offset = expect_number() * sign;
  }
  node->endline = TRUE;
  return node;
}

Node *new_struct(Token *tok) {
  Node *node = new_node(ND_STRUCT);
  StructTag *struct_tag = find_struct_tag(tok);
  if (!struct_tag) {
    error_at(tok->str, "unknown tag: %.*s [in struct declaration]", tok->len, tok->str);
  }
  Struct *struct_ = struct_tag->main;
  struct_->var = malloc(sizeof(LVar));
  struct_->var->next = NULL;
  struct_->var->type = new_type(TY_NONE);
  int offset = 0;
  expect("{", "before struct members", "struct");
  while (token->kind != TK_EOF && !(token->kind == TK_RESERVED && !memcmp(token->str, "}", token->len))) {
    Type *type = consume_type();
    Token *member_tok = consume_ident();
    if (!member_tok) {
      error_at(token->str, "expected an identifier but got \"%.*s\" [in struct declaration]", token->len, token->str);
    }
    while (consume("[")) {
      type = new_type_arr(type, expect_number());
      expect("]", "after number", "array declaration");
    }
    LVar *member_var = malloc(sizeof(LVar));
    member_var->name = member_tok->str;
    member_var->len = member_tok->len;
    member_var->type = type;
    member_var->next = struct_->var;
    struct_->var = member_var;
    int type_size = get_sizeof(type);
    int single_size;
    if (type->ty == TY_ARR) {
      single_size = get_type_size(type->ptr_to);
    } else {
      single_size = type_size;
    }
    if (offset % single_size != 0) {
      offset += single_size - (offset % single_size);
    }
    member_var->offset = offset;
    offset += type_size;
    if (consume(";")) {
      continue;
    } else {
      error_at(token->str, "expected ';' after struct member declaration [in struct declaration]");
    }
  }
  struct_->size = offset;
  expect("}", "after struct members", "struct");
  expect(";", "after struct definition", "struct");
  node->type = new_type_struct(struct_);
  node->endline = TRUE;
  return node;
}

Node *stmt() {
  Node *node;
  Token *tok;
  Type *type;
  int loop_id_prev;
  if (consume("{")) {
    node = new_node(ND_BLOCK);
    LVar *var = current_fn->locals;
    node->body = malloc(sizeof(Node *));
    int i = 0;
    while (!(token->kind == TK_RESERVED && !memcmp(token->str, "}", token->len))) {
      node->body[i++] = stmt();
      node->body = realloc(node->body, sizeof(Node *) * (i + 1));
      if (!node->body)
        error("realloc failed");
    }
    node->body[i] = new_node(ND_NONE);
    current_fn->locals = var;
    expect("}", "after block", "block");
  } else if (is_type(token)) {
    // 変数宣言または関数定義
    Type *ch_type = check_type();
    type = consume_type();
    tok = consume_ident();
    if (!tok) {
      error_at(token->str, "expected an identifier but got \"%.*s\" [in variable declaration]", token->len, token->str);
    }
    if (consume("(")) {
      // 関数定義
      if (current_fn->next) {
        error_at(token->str, "nested function is not supported [in function definition]");
      }
      node = function_definition(tok, type);
    } else if (current_fn->next) {
      // ローカル変数宣言
      node = new_node(ND_BLOCK);
      node->body = malloc(sizeof(Node *));
      int i = 0;
      node->body[i++] = local_variable_declaration(tok, type);
      while (consume(",")) {
        type = ch_type;
        while (consume("*")) {
          Type *ptr = malloc(sizeof(Type));
          ptr->ty = TY_PTR;
          ptr->ptr_to = type;
          type = ptr;
        }
        tok = consume_ident();
        if (!tok) {
          error_at(token->str, "expected an identifier but got \"%.*s\" [in variable declaration]", token->len,
                   token->str);
        }
        node->body[i++] = local_variable_declaration(tok, type);
        node->body = realloc(node->body, sizeof(Node *) * (i + 1));
        if (!node->body)
          error("realloc failed");
      }
      node->body[i] = new_node(ND_NONE);
      expect(";", "after line", "local variable declaration");
    } else {
      // グローバル変数宣言
      node = new_node(ND_BLOCK);
      node->body = malloc(sizeof(Node *));
      int i = 0;
      node->body[i++] = global_variable_declaration(tok, type);
      while (consume(",")) {
        type = ch_type;
        while (consume("*")) {
          Type *ptr = malloc(sizeof(Type));
          ptr->ty = TY_PTR;
          ptr->ptr_to = type;
          type = ptr;
        }
        tok = consume_ident();
        if (!tok) {
          error_at(token->str, "expected an identifier but got \"%.*s\" [in variable declaration]", token->len,
                   token->str);
        }
        node->body[i++] = global_variable_declaration(tok, type);
        node->body = realloc(node->body, sizeof(Node *) * (i + 1));
        if (!node->body)
          error("realloc failed");
      }
      node->body[i] = new_node(ND_NONE);
      expect(";", "after line", "global variable declaration");
    }
  } else if (token->kind == TK_STRUCT) {
    token = token->next;
    tok = consume_ident();
    if (!tok) {
      error_at(token->str, "expected an identifier but got \"%.*s\" [in struct definition]", token->len, token->str);
    }
    node = new_struct(tok);
  } else if (token->kind == TK_TYPEDEF) {
    token = token->next;
    if (token->kind == TK_STRUCT) {
      token = token->next;
      node = new_node(ND_TYPEDEF);
      Token *tok1 = consume_ident();
      if (!tok1) {
        error_at(token->str, "expected an identifier but got \"%.*s\" [in typedef]", token->len, token->str);
      }
      Token *tok2 = consume_ident();
      if (!tok2) {
        error_at(token->str, "expected an identifier but got \"%.*s\" [in typedef]", token->len, token->str);
      }
      if (find_struct_tag(tok1)) {
        error_at(tok1->str, "duplicated tag name: %.*s [in typedef]", tok1->len, tok1->str);
      }
      if (find_struct(tok2)) {
        error_at(tok2->str, "duplicated struct name: %.*s [in typedef]", tok2->len, tok2->str);
      }
      Struct *var = malloc(sizeof(Struct));
      var->name = tok2->str;
      var->len = tok2->len;
      var->next = structs;
      structs = var;
      StructTag *tag = malloc(sizeof(StructTag));
      tag->name = tok1->str;
      tag->len = tok1->len;
      tag->main = var;
      tag->next = struct_tags;
      struct_tags = tag;
    } else if (token->kind == TK_ENUM) {
      token = token->next;
      node = new_node(ND_TYPEDEF);
      int offset = 0;
      expect("{", "before enum members", "enum");
      while (token->kind != TK_EOF && !(token->kind == TK_RESERVED && !memcmp(token->str, "}", token->len))) {
        Token *member_tok = consume_ident();
        if (!member_tok) {
          error_at(token->str, "expected an identifier but got \"%.*s\" [in typedef]", token->len, token->str);
        }
        LVar *member_var = malloc(sizeof(LVar));
        member_var->name = member_tok->str;
        member_var->len = member_tok->len;
        member_var->type = new_type(TY_INT);
        member_var->offset = offset;
        offset++;
        member_var->next = enum_members;
        enum_members = member_var;
        if (consume(",")) {
          continue;
        } else if (consume("}")) {
          break;
        } else {
          error_at(token->str, "expected ',' after member declaration [in typedef]");
        }
      }
      tok = consume_ident();
      if (!tok) {
        error_at(token->str, "expected an identifier but got \"%.*s\" [in typedef]", token->len, token->str);
      } else if (find_enum(tok)) {
        error_at(tok->str, "duplicated enum name: %.*s [in typedef]", tok->len, tok->str);
      }
      Enum *enum_ = malloc(sizeof(Enum));
      enum_->name = tok->str;
      enum_->len = tok->len;
      enum_->next = enums;
      enums = enum_;
    } else {
      error_at(token->str, "expected a struct but got \"%.*s\" [in typedef]", token->len, token->str);
    }
    node->endline = TRUE;
    expect(";", "after line", "typedef");
  } else if (token->kind == TK_IF) {
    token = token->next;
    node = new_node(ND_IF);
    node->id = loop_cnt++;
    expect("(", "before condition", "if");
    node->cond = expr();
    node->cond->endline = FALSE;
    expect(")", "after equality", "if");
    node->then = stmt();
    if (consume("else")) {
      node->els = stmt();
    } else {
      node->els = NULL;
    }
  } else if (token->kind == TK_WHILE) {
    token = token->next;
    expect("(", "before condition", "while");
    node = new_node(ND_WHILE);
    node->id = loop_cnt++;
    node->cond = expr();
    node->cond->endline = FALSE;
    expect(")", "after equality", "while");
    loop_id_prev = loop_id;
    loop_id = node->id;
    node->then = stmt();
    loop_id = loop_id_prev;
  } else if (token->kind == TK_DO) {
    token = token->next;
    node = new_node(ND_DOWHILE);
    node->id = loop_cnt++;
    node->then = stmt();
    if (token->kind != TK_WHILE) {
      error_at(token->str, "expected 'while' but got \"%.*s\" [in do-while statement]", token->len, token->str);
    }
    token = token->next;
    expect("(", "before condition", "do-while");
    node->cond = expr();
    node->cond->endline = FALSE;
    expect(")", "after equality", "do-while");
    loop_id_prev = loop_id;
    loop_id = node->id;
    loop_id = loop_id_prev;
    expect(";", "after line", "do-while");
    node->endline = TRUE;
  } else if (token->kind == TK_FOR) {
    int init;
    token = token->next;
    expect("(", "before initialization", "for");
    node = new_node(ND_FOR);
    node->id = loop_cnt++;
    if (consume(";")) {
      node->init = new_node(ND_NONE);
      node->init->endline = TRUE;
      init = FALSE;
    } else if (is_type(token)) {
      type = consume_type();
      tok = consume_ident();
      if (!tok) {
        error_at(token->str, "expected an identifier but got \"%.*s\" [in variable declaration]", token->len,
                 token->str);
      }
      node->init = local_variable_declaration(tok, type);
      expect(";", "after initialization", "for");
      node->init->endline = TRUE;
      init = TRUE;
    } else {
      node->init = expr();
      expect(";", "after initialization", "for");
      node->init->endline = TRUE;
      init = FALSE;
    }
    if (consume(";")) {
      node->cond = new_num(1);
      node->cond->endline = FALSE;
    } else {
      node->cond = expr();
      node->cond->endline = FALSE;
      expect(";", "after condition", "for");
    }
    if (consume(")")) {
      node->step = new_node(ND_NONE);
      node->step->endline = TRUE;
    } else {
      node->step = expr();
      node->step->endline = TRUE;
      expect(")", "after step expression", "for");
    }
    loop_id_prev = loop_id;
    loop_id = node->id;
    node->then = stmt();
    if (init) {
      current_fn->locals = current_fn->locals->next;
    }
    loop_id = loop_id_prev;
  } else if (token->kind == TK_BREAK) {
    if (loop_id == -1) {
      error_at(token->str, "stray break statement [in break statement]");
    }
    token = token->next;
    expect(";", "after line", "break");
    node = new_node(ND_BREAK);
    node->endline = TRUE;
    node->id = loop_id;
  } else if (token->kind == TK_CONTINUE) {
    if (loop_id == -1) {
      error_at(token->str, "stray continue statement [in continue statement]");
    }
    token = token->next;
    expect(";", "after line", "continue");
    node = new_node(ND_CONTINUE);
    node->endline = TRUE;
    node->id = loop_id;
  } else if (token->kind == TK_RETURN) {
    token = token->next;
    node = new_node(ND_RETURN);
    if (consume(";")) {
      node->rhs = new_num(0);
    } else {
      node->rhs = expr();
      expect(";", "after line", "return");
    }
    node->endline = TRUE;
  } else {
    node = expr();
    expect(";", "after line", "expression");
    node->endline = TRUE;
  }
  return node;
}

void program() {
  int i = 0;
  code = malloc(sizeof(Node *));
  while (token->kind != TK_EOF) {
    code[i++] = stmt();
    code = realloc(code, sizeof(Node *) * (i + 1));
    if (!code)
      error("realloc failed");
  }
  code[i] = new_node(ND_NONE);
}

Node *expr() { return assign(); }

Node *assign() {
  Node *node = logical_or();
  if (consume("=")) {
    node = new_binary(ND_ASSIGN, node, expr());
  } else if (consume("+=")) {
    node = new_binary(ND_ASSIGN, node, new_binary(ND_ADD, node, expr()));
  } else if (consume("-=")) {
    node = new_binary(ND_ASSIGN, node, new_binary(ND_SUB, node, expr()));
  } else if (consume("*=")) {
    node = new_binary(ND_ASSIGN, node, new_binary(ND_MUL, node, expr()));
  } else if (consume("/=")) {
    node = new_binary(ND_ASSIGN, node, new_binary(ND_DIV, node, expr()));
  } else if (consume("%=")) {
    node = new_binary(ND_ASSIGN, node, new_binary(ND_MOD, node, expr()));
  }
  return node;
}

Node *logical_or() {
  Node *node = logical_and();
  for (;;) {
    if (consume("||")) {
      node = new_binary(ND_OR, node, logical_and());
      node->type = node->lhs->type;
    } else {
      break;
    }
  }
  return node;
}

Node *logical_and() {
  Node *node = bit_operator();
  for (;;) {
    if (consume("&&")) {
      node = new_binary(ND_AND, node, bit_operator());
      node->type = node->lhs->type;
    } else {
      break;
    }
  }
  return node;
}

Node *bit_operator() {
  Node *node = equality();
  for (;;) {
    if (consume("&")) {
      node = new_binary(ND_BITAND, node, equality());
      node->type = node->lhs->type;
    } else if (consume("|")) {
      node = new_binary(ND_BITOR, node, equality());
      node->type = node->lhs->type;
    } else if (consume("^")) {
      node = new_binary(ND_BITXOR, node, equality());
      node->type = node->lhs->type;
    } else {
      break;
    }
  }
  return node;
}

// equality = relational ("==" relational | "!=" relational)*
Node *equality() {
  Node *node = relational();

  for (;;) {
    if (consume("==")) {
      node = new_binary(ND_EQ, node, relational());
      node->type = new_type(TY_INT);
    } else if (consume("!=")) {
      node = new_binary(ND_NE, node, relational());
      node->type = new_type(TY_INT);
    } else {
      break;
    }
  }
  return node;
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
Node *relational() {
  Node *node = bit_shift();

  for (;;) {
    if (consume("<")) {
      node = new_binary(ND_LT, node, bit_shift());
      node->type = new_type(TY_INT);
    } else if (consume("<=")) {
      node = new_binary(ND_LE, node, bit_shift());
      node->type = new_type(TY_INT);
    } else if (consume(">")) {
      node = new_binary(ND_LT, bit_shift(), node);
      node->type = new_type(TY_INT);
    } else if (consume(">=")) {
      node = new_binary(ND_LE, bit_shift(), node);
      node->type = new_type(TY_INT);
    } else {
      break;
    }
  }
  return node;
}

Node *bit_shift() {
  Node *node = add();
  for (;;) {
    if (consume("<<")) {
      node = new_binary(ND_SHL, node, add());
      node->type = node->lhs->type;
    } else if (consume(">>")) {
      node = new_binary(ND_SHR, node, add());
      node->type = node->lhs->type;
    } else {
      break;
    }
  }
  return node;
}

// ポインタ + 整数 または 整数 + ポインタ の場合に
// 整数側を型サイズで乗算するラッパ
Node *new_add(Node *lhs, Node *rhs, char *ptr) {
  Node *node;
  Node *mul_node;
  // lhsがptr, rhsがptrなら
  if (is_ptr_or_arr(lhs->type) && is_ptr_or_arr(rhs->type)) {
    error_at(ptr, "invalid operands to binary expression [in new_add]");
  }
  // lhsがptr, rhsがintなら
  else if (is_ptr_or_arr(lhs->type) && is_number(rhs->type)) {
    mul_node = new_binary(ND_MUL, rhs, new_num(get_sizeof(lhs->type->ptr_to)));
    node = new_binary(ND_ADD, lhs, mul_node);
    node->type = lhs->type;
  }
  // lhsがint, rhsがptrなら
  else if (is_number(lhs->type) && is_ptr_or_arr(rhs->type)) {
    mul_node = new_binary(ND_MUL, lhs, new_num(get_sizeof(rhs->type->ptr_to)));
    node = new_binary(ND_ADD, mul_node, rhs);
    node->type = rhs->type;
  }
  // それ以外は普通に演算
  else {
    node = new_binary(ND_ADD, lhs, rhs);
    node->type = new_type(TY_INT);
  }
  return node;
}

Node *new_sub(Node *lhs, Node *rhs, char *ptr) {
  Node *node;
  Node *mul_node;

  // lhsがptr, rhsがptrなら
  if (is_ptr_or_arr(lhs->type) && is_ptr_or_arr(rhs->type)) {
    node = new_binary(ND_SUB, lhs, rhs);
    node->type = new_type(TY_INT);
  }
  // lhsがint, rhsがptrなら
  else if (is_number(lhs->type) && is_ptr_or_arr(rhs->type)) {
    error_at(ptr, "invalid operands to binary expression [in new_sub]");
  }
  // lhsがptr, rhsがintなら
  else if (is_ptr_or_arr(lhs->type) && is_number(rhs->type)) {
    mul_node = new_binary(ND_MUL, rhs, new_num(get_sizeof(lhs->type->ptr_to)));
    node = new_binary(ND_SUB, lhs, mul_node);
    node->type = lhs->type;
  }
  // それ以外は普通に演算
  else {
    node = new_binary(ND_SUB, lhs, rhs);
    node->type = new_type(TY_INT);
  }
  return node;
}

// add = mul ("+" mul | "-" mul)*
// ポインタ演算を挟み込む
Node *add() {
  char *consumed_ptr_prev;
  Node *node = mul();
  for (;;) {
    if (consume("+")) {
      consumed_ptr_prev = consumed_ptr;
      node = new_add(node, mul(), consumed_ptr_prev);
    } else if (consume("-")) {
      consumed_ptr_prev = consumed_ptr;
      node = new_sub(node, mul(), consumed_ptr_prev);
    } else {
      break;
    }
  }
  return node;
}

Type *resolve_type_mul(Type *left, Type *right, char *ptr) {
  if (is_number(left) && is_number(right)) {
    return new_type(TY_INT);
  }
  error_at(ptr, "invalid operands to binary expression [in resolve_type_mul]");
  return NULL;
}

// mul = unary ("*" unary | "/" unary)*
Node *mul() {
  char *consumed_ptr_prev;
  Node *node = unary();

  for (;;) {
    if (consume("*")) {
      consumed_ptr_prev = consumed_ptr;
      node = new_binary(ND_MUL, node, unary());
      node->type = resolve_type_mul(node->lhs->type, node->rhs->type, consumed_ptr_prev);
    } else if (consume("/")) {
      consumed_ptr_prev = consumed_ptr;
      node = new_binary(ND_DIV, node, unary());
      node->type = resolve_type_mul(node->lhs->type, node->rhs->type, consumed_ptr_prev);
    } else if (consume("%")) {
      consumed_ptr_prev = consumed_ptr;
      node = new_binary(ND_MOD, node, unary());
      node->type = resolve_type_mul(node->lhs->type, node->rhs->type, consumed_ptr_prev);
    } else {
      break;
    }
  }
  return node;
}

// unary = ("+" | "-")? unary
//       | primary
Node *unary() {
  Node *node;
  if (consume("sizeof")) {
    return new_num(get_sizeof(unary()->type));
  }
  if (consume("+"))
    return increment_decrement();
  if (consume("-")) {
    node = new_binary(ND_SUB, new_num(0), increment_decrement());
    node->type = node->rhs->type;
    return node;
  }
  if (consume("&")) {
    node = new_node(ND_ADDR);
    node->lhs = unary();
    node->type = new_type_ptr(node->lhs->type);
    return node;
  }
  if (consume("*")) {
    char *consumed_ptr_prev = consumed_ptr;
    node = unary();
    node = new_deref(node);
    if (!is_ptr_or_arr(node->lhs->type)) {
      error_at(consumed_ptr_prev, "invalid pointer dereference");
    }
    return node;
  }
  if (consume("!")) {
    node = new_node(ND_NOT);
    node->lhs = unary();
    node->type = node->lhs->type;
    return node;
  }
  if (consume("~")) {
    node = new_node(ND_BITNOT);
    node->lhs = unary();
    node->type = node->lhs->type;
    return node;
  }
  return increment_decrement();
}

Node *increment_decrement() {
  Node *node;
  if (consume("++")) {
    node = access_member();
    return new_binary(ND_ASSIGN, node, new_add(node, new_num(1), consumed_ptr));
  } else if (consume("--")) {
    node = access_member();
    return new_binary(ND_ASSIGN, node, new_sub(node, new_num(1), consumed_ptr));
  }
  node = access_member();
  if (consume("++")) {
    node = new_binary(ND_POSTINC, node, new_add(node, new_num(1), consumed_ptr));
  } else if (consume("--")) {
    node = new_binary(ND_POSTINC, node, new_sub(node, new_num(1), consumed_ptr));
  }
  return node;
}

// Structure Reference and Array Indexing
Node *access_member() {
  Node *ptr;
  Node *offset_node;
  Token *tok;
  Struct *struct_;
  LVar *var;
  char *consumed_ptr_prev;
  Token *prev_tok = token;
  Node *node = primary();
  for (;;) {
    if (consume("[")) {
      consumed_ptr_prev = consumed_ptr;
      if (!is_ptr_or_arr(node->type)) {
        error_at(consumed_ptr_prev, "invalid array access [in primary]");
      }
      node = new_add(node, expr(), consumed_ptr_prev);
      expect("]", "after number", "array access");
      node = new_deref(node);
    } else if (consume(".")) {
      if (node->type->ty != TY_STRUCT) {
        error_at(prev_tok->str, "%.*s is not a struct [in struct reference]", prev_tok->len, prev_tok->str);
      }
      tok = consume_ident();
      if (!tok) {
        error_at(token->str, "expected an identifier but got \"%.*s\" [in struct reference]", token->len, token->str);
      }
      struct_ = node->type->struct_;
      if (!struct_) {
        error_at(prev_tok->str, "unknown struct: %.*s [in struct reference]", prev_tok->len, prev_tok->str);
      } else if (!struct_->size) {
        error_at(prev_tok->str, "not initialized struct: %.*s [in struct reference]", prev_tok->len, prev_tok->str);
      }
      var = find_struct_member(struct_, tok);
      offset_node = new_num(var->offset);
      ptr = new_node(ND_ADDR);
      ptr->lhs = node;
      ptr->type = new_type_ptr(node->type);
      ptr = new_binary(ND_ADD, ptr, offset_node);
      ptr->type = new_type_ptr(var->type);
      node = new_deref(ptr);
    } else if (consume("->")) {
      if (node->type->ty != TY_PTR) {
        error_at(prev_tok->str, "%.*s is not a pointer [in struct reference]", prev_tok->len, prev_tok->str);
      }
      if (node->type->ptr_to->ty != TY_STRUCT) {
        error_at(prev_tok->str, "%.*s is not a pointer of a struct [in struct reference]", prev_tok->len,
                 prev_tok->str);
      }
      tok = consume_ident();
      if (!tok) {
        error_at(token->str, "expected an identifier but got \"%.*s\" [in struct reference]", token->len, token->str);
      }
      struct_ = node->type->ptr_to->struct_;
      if (!struct_) {
        error_at(prev_tok->str, "unknown struct: %.*s [in struct reference]", prev_tok->len, prev_tok->str);
      } else if (!struct_->size) {
        error_at(prev_tok->str, "not initialized struct: %.*s [in struct reference]", prev_tok->len, prev_tok->str);
      }
      var = find_struct_member(struct_, tok);
      offset_node = new_num(var->offset);
      ptr = new_binary(ND_ADD, node, offset_node);
      ptr->type = new_type_ptr(var->type);
      node = new_deref(ptr);
    } else
      break;
  }
  return node;
}

// primary = "(" expr ")" | num
Node *primary() {
  Node *node;
  Token *tok;
  Type *type;

  // 括弧
  if (consume("(")) {
    node = expr();
    expect(")", "after expression", "primary");
    return node;
  }

  // 数値
  if (token->kind == TK_NUM) {
    return new_num(expect_number());
  }

  // 文字列
  if (token->kind == TK_STR) {
    String *str = malloc(sizeof(String));
    str->text = token->str;
    str->len = token->len;
    str->label = label_cnt++;
    str->next = strings;
    strings = str;
    token = token->next;
    node = new_node(ND_STR);
    node->str = str;
    node->type = new_type_ptr(new_type(TY_CHAR));
    return node;
  }

  // 型
  type = consume_type();
  if (type) {
    node = new_node(ND_TYPE);
    node->type = type;
    return node;
  }

  tok = consume_ident();
  if (!tok) {
    error_at(token->str, "expected an identifier but got \"%.*s\" [in primary]", token->len, token->str);
    return NULL;
  }

  // enumのメンバー
  LVar *member = find_enum_member(tok);
  if (member) {
    node = new_num(member->offset);
    return node;
  }

  // 変数
  if (!consume("(")) {
    LVar *lvar = find_lvar(tok);
    LVar *gvar = find_gver(tok);
    if (lvar) {
      node = new_node(ND_LVAR);
      node->var = lvar;
      node->type = lvar->type;
    } else if (gvar) {
      node = new_node(ND_GVAR);
      node->var = gvar;
      node->type = gvar->type;

    } else {
      error_at(tok->str, "undefined variable: %.*s [in primary]", tok->len, tok->str);
    }
    return node;
  }

  // 関数呼び出し
  else {
    Function *fn = find_fn(tok);
    if (!fn) {
      error_at(tok->str, "undefined function: %.*s [in primary]", tok->len, tok->str);
    }
    node = new_node(ND_FUNCALL);
    node->fn = fn;
    node->id = loop_cnt++;
    node->type = fn->type;
    if (!consume(")")) {
      int n = 0;
      for (int i = 0; i < 6; i++) {
        node->args[i] = expr();
        n += 1;
        if (!consume(","))
          break;
      }
      node->val = n;
      expect(")", "after arguments", "function call");
    } else {
      node->val = 0;
    }
    return node;
  }
}

//
// Code generator
//

char *regs1(int i) {
  if (i == 0)
    return "dil";
  else if (i == 1)
    return "sil";
  else if (i == 2)
    return "dl";
  else if (i == 3)
    return "cl";
  else if (i == 4)
    return "r8b";
  else if (i == 5)
    return "r9b";
  else
    return NULL;
}

char *regs4(int i) {
  if (i == 0)
    return "edi";
  else if (i == 1)
    return "esi";
  else if (i == 2)
    return "edx";
  else if (i == 3)
    return "ecx";
  else if (i == 4)
    return "r8d";
  else if (i == 5)
    return "r9d";
  else
    return NULL;
}

char *regs8(int i) {
  if (i == 0)
    return "rdi";
  else if (i == 1)
    return "rsi";
  else if (i == 2)
    return "rdx";
  else if (i == 3)
    return "rcx";
  else if (i == 4)
    return "r8";
  else if (i == 5)
    return "r9";
  else
    return NULL;
}

void gen_lval(Node *node) {
  if (node->kind == ND_LVAR || node->kind == ND_VARDEC) {
    printf("  mov rax, rbp\n");
    printf("  sub rax, %d\n", node->var->offset);
    printf("  push rax\n");
  } else if (node->kind == ND_GVAR) {
    printf("  lea rax, %.*s[rip]\n", node->var->len, node->var->name);
    printf("  push rax\n");
  } else if (node->kind == ND_DEREF) {
    gen(node->lhs);
  } else {
    error("invalid lvalue, node->kind: %d\n", node->kind);
  }
}

void gen(Node *node) {
  if (node->kind == ND_NUM) {
    if (!node->endline)
      printf("  push %d\n", node->val);
    return;
  } else if (node->kind == ND_STR) {
    printf("  lea rax, [rip + .L.str%d]\n", node->str->label);
    if (!node->endline)
      printf("  push rax\n");
    return;
  } else if ((node->kind == ND_LVAR) || (node->kind == ND_GVAR)) {
    gen_lval(node);
    printf("  pop rax\n");
    if (node->type->ty == TY_INT) {
      printf("  movsxd rax, DWORD PTR [rax]\n");
    } else if (node->type->ty == TY_CHAR) {
      printf("  movzx rax, BYTE PTR [rax]\n");
    } else if (node->type->ty == TY_PTR) {
      printf("  mov rax, QWORD PTR [rax]\n");
    } else if (node->type->ty == TY_ARR) {
    } else {
      error("invalid type [in ND_LVAR]");
    }
    if (!node->endline)
      printf("  push rax\n");
    return;
  } else if (node->kind == ND_ADDR) {
    gen_lval(node->lhs);
    return;
  } else if (node->kind == ND_DEREF) {
    gen(node->lhs);
    printf("  pop rax\n");
    if (node->type->ty == TY_INT) {
      printf("  movsxd rax, DWORD PTR [rax]\n");
    } else if (node->type->ty == TY_CHAR) {
      printf("  movzx rax, BYTE PTR [rax]\n");
    } else if (node->type->ty == TY_PTR) {
      printf("  mov rax, QWORD PTR [rax]\n");
    } else if (node->type->ty == TY_ARR) {
    } else {
      error("invalid type [in ND_DEREF]");
    }
    if (!node->endline)
      printf("  push rax\n");
    return;
  } else if (node->kind == ND_NOT) {
    gen(node->lhs);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    printf("  sete al\n");
    printf("  movzx rax, al\n");
    if (!node->endline)
      printf("  push rax\n");
    return;
  } else if (node->kind == ND_BITNOT) {
    gen(node->lhs);
    printf("  pop rax\n");
    printf("  not rax\n");
    if (!node->endline)
      printf("  push rax\n");
    return;
  } else if (node->kind == ND_ASSIGN) {
    gen_lval(node->lhs);
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");
    if (node->lhs->type->ty == TY_INT) {
      printf("  mov DWORD PTR [rax], edi\n");
    } else if (node->lhs->type->ty == TY_CHAR) {
      printf("  mov BYTE PTR [rax], dil\n");
    } else if (node->lhs->type->ty == TY_PTR) {
      printf("  mov QWORD PTR [rax], rdi\n");
    } else {
      error("invalid type [in ND_ASSIGN]");
    }
    if (!node->endline)
      printf("  push rdi\n");
    return;
  } else if (node->kind == ND_POSTINC) {
    gen_lval(node->lhs);
    if (!node->endline) {
      printf("  pop rax\n");
      if (node->type->ty == TY_INT) {
        printf("  movsxd rdi, DWORD PTR [rax]\n");
      } else if (node->type->ty == TY_CHAR) {
        printf("  movzx rdi, BYTE PTR [rax]\n");
      } else if (node->type->ty == TY_PTR) {
        printf("  mov rdi, QWORD PTR [rax]\n");
      } else {
        error("invalid type [in ND_POSTINC]");
      }
      printf("  push rdi\n");
      printf("  push rax\n");
    }
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");
    if (node->lhs->type->ty == TY_INT) {
      printf("  mov DWORD PTR [rax], edi\n");
    } else if (node->lhs->type->ty == TY_CHAR) {
      printf("  mov BYTE PTR [rax], dil\n");
    } else if (node->lhs->type->ty == TY_PTR) {
      printf("  mov QWORD PTR [rax], rdi\n");
    } else {
      error("invalid type [in ND_ASSIGN]");
    }
    return;
  } else if (node->kind == ND_RETURN) {
    gen(node->rhs);
    printf("  pop rax\n");
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
    return;
  } else if (node->kind == ND_BLOCK) {
    for (int i = 0; node->body[i]->kind != ND_NONE; i++) {
      gen(node->body[i]);
    }
    return;
  } else if (node->kind == ND_IF) {
    gen(node->cond);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    if (node->els) {
      printf("  je .Lelse%d\n", node->id);
      gen(node->then);
      printf("  jmp .Lend%d\n", node->id);
      printf(".Lelse%d:\n", node->id);
      gen(node->els);
      printf(".Lend%d:\n", node->id);
    } else {
      printf("  je .Lend%d\n", node->id);
      gen(node->then);
      printf(".Lend%d:\n", node->id);
    }
    return;
  } else if (node->kind == ND_WHILE) {
    printf(".Lbegin%d:\n", node->id);
    gen(node->cond);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    printf("  je .Lend%d\n", node->id);
    gen(node->then);
    printf(".Lstep%d:\n", node->id);
    printf("  jmp .Lbegin%d\n", node->id);
    printf(".Lend%d:\n", node->id);
    return;
  } else if (node->kind == ND_DOWHILE) {
    printf(".Lbegin%d:\n", node->id);
    gen(node->then);
    gen(node->cond);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    printf("  je .Lend%d\n", node->id);
    printf(".Lstep%d:\n", node->id);
    printf("  jmp .Lbegin%d\n", node->id);
    printf(".Lend%d:\n", node->id);
    return;
  } else if (node->kind == ND_FOR) {
    gen(node->init);
    printf(".Lbegin%d:\n", node->id);
    gen(node->cond);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    printf("  je .Lend%d\n", node->id);
    gen(node->then);
    printf(".Lstep%d:\n", node->id);
    gen(node->step);
    printf("  jmp .Lbegin%d\n", node->id);
    printf(".Lend%d:\n", node->id);
    return;
  } else if (node->kind == ND_BREAK) {
    printf("  jmp .Lend%d\n", node->id);
    return;
  } else if (node->kind == ND_CONTINUE) {
    printf("  jmp .Lstep%d\n", node->id);
    return;
  } else if (node->kind == ND_FUNCDEF) {
    printf("  .globl %.*s\n", node->fn->len, node->fn->name);
    printf("  .p2align 4\n");
    printf("%.*s:\n", node->fn->len, node->fn->name);
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    int offset;
    if (node->fn->offset % 8) {
      offset = (node->fn->offset / 8 + 1) * 8;
    } else {
      offset = node->fn->offset;
    }
    printf("  sub rsp, %d\n", offset);
    for (int i = 0; i < node->val; i++) {
      gen_lval(node->args[i]);
      printf("  pop rax\n");
      if (node->args[i]->type->ty == TY_INT) {
        printf("  mov DWORD PTR [rax], %s\n", regs4(i));
      } else if (node->args[i]->type->ty == TY_CHAR) {
        printf("  mov BYTE PTR [rax], %s\n", regs1(i));
      } else if (node->args[i]->type->ty == TY_PTR) {
        printf("  mov QWORD PTR [rax], %s\n", regs8(i));
      } else {
        error("invalid type [in ND_FUNCDEF]");
      }
    }
    gen(node->lhs);
    if (node->fn->type->ty == TY_VOID) {
      printf("  mov rsp, rbp\n");
      printf("  pop rbp\n");
      printf("  ret\n");
    }
    return;
  } else if (node->kind == ND_FUNCALL) {
    for (int i = 0; i < node->val; i++) {
      gen(node->args[i]);
    }
    for (int i = node->val - 1; i >= 0; i--) {
      printf("  pop rax\n");
      if (node->args[i]->type->ty == TY_INT) {
        printf("  mov %s, eax\n", regs4(i));
      } else if (node->args[i]->type->ty == TY_CHAR) {
        printf("  movzx %s, al\n", regs4(i));
      } else if (is_ptr_or_arr(node->args[i]->type)) {
        printf("  mov %s, rax\n", regs8(i));
      } else {
        error("invalid type [in ND_FUNCALL]");
      }
    }
    // Align stack
    printf("  mov rax, rsp\n");
    printf("  and rax, 0xF\n");
    printf("  cmp rax, 0\n");
    printf("  je .Laligned%d\n", node->id);
    printf("  sub rsp, 8\n");
    printf("  jmp .Lfixup%d\n", node->id);
    printf(".Laligned%d:\n", node->id);
    printf("  mov rax, 0\n");
    printf("  call %.*s\n", node->fn->len, node->fn->name);
    printf("  jmp .Lend%d\n", node->id);
    printf(".Lfixup%d:\n", node->id);
    printf("  call %.*s\n", node->fn->len, node->fn->name);
    printf("  add rsp, 8\n");
    printf(".Lend%d:\n", node->id);
    if (!node->endline)
      printf("  push rax\n");
    return;
  } else if ((node->kind == ND_GLBDEC) || (node->kind == ND_VARDEC) || (node->kind == ND_EXTERN) ||
             (node->kind == ND_TYPEDEF) || (node->kind == ND_ENUM) || (node->kind == ND_STRUCT) ||
             (node->kind == ND_TYPE || node->kind == ND_NONE)) {
    return;
  }

  gen(node->lhs);
  gen(node->rhs);

  printf("  pop rdi\n");
  printf("  pop rax\n");

  if (node->kind == ND_ADD) {
    printf("  add rax, rdi\n");
  } else if (node->kind == ND_SUB) {
    printf("  sub rax, rdi\n");
  } else if (node->kind == ND_MUL) {
    printf("  imul rax, rdi\n");
  } else if (node->kind == ND_DIV) {
    printf("  cqo\n");
    printf("  idiv rdi\n");
  } else if (node->kind == ND_MOD) {
    printf("  cqo\n");
    printf("  idiv rdi\n");
    printf("  mov rax, rdx\n");
  } else if (node->kind == ND_EQ) {
    printf("  cmp rax, rdi\n");
    printf("  sete al\n");
    printf("  movzx rax, al\n");
  } else if (node->kind == ND_NE) {
    printf("  cmp rax, rdi\n");
    printf("  setne al\n");
    printf("  movzx rax, al\n");
  } else if (node->kind == ND_LT) {
    printf("  cmp rax, rdi\n");
    printf("  setl al\n");
    printf("  movzx rax, al\n");
  } else if (node->kind == ND_LE) {
    printf("  cmp rax, rdi\n");
    printf("  setle al\n");
    printf("  movzx rax, al\n");
  } else if (node->kind == ND_AND) {
    printf("  test rax, rax\n");
    printf("  setne al\n");
    printf("  test rdi, rdi\n");
    printf("  setne dl\n");
    printf("  and al, dl\n");
    printf("  movzx rax, al\n");
  } else if (node->kind == ND_OR) {
    printf("  test rax, rax\n");
    printf("  setne al\n");
    printf("  test rdi, rdi\n");
    printf("  setne dl\n");
    printf("  or al, dl\n");
    printf("  movzx rax, al\n");
  } else if (node->kind == ND_BITAND) {
    printf("  and rax, rdi\n");
  } else if (node->kind == ND_BITOR) {
    printf("  or rax, rdi\n");
  } else if (node->kind == ND_BITXOR) {
    printf("  xor rax, rdi\n");
  } else if (node->kind == ND_SHL) {
    // シフト量は CL レジスタで指定する
    printf("  mov rcx, rdi\n");
    printf("  shl rax, cl\n");
  } else if (node->kind == ND_SHR) {
    // シフト量は CL レジスタで指定する
    printf("  mov rcx, rdi\n");
    printf("  sar rax, cl\n");
  } else {
    error("invalid node kind");
  }

  if (!node->endline)
    printf("  push rax\n");
}

void init_global_variables() {
  // functionの初期化
  functions = malloc(sizeof(Function));
  functions->next = NULL;
  current_fn = functions;

  // enumの初期化
  enums = malloc(sizeof(Enum));
  enums->next = NULL;
  enum_members = malloc(sizeof(LVar));
  enum_members->next = NULL;
  enum_members->type = new_type(TY_NONE);

  // 構造体の初期化
  structs = malloc(sizeof(Struct));
  structs->next = NULL;
  struct_tags = malloc(sizeof(StructTag));
  struct_tags->next = NULL;

  // グローバル変数の初期化
  globals = malloc(sizeof(LVar));
  globals->next = NULL;
  globals->type = new_type(TY_NONE);

  // 文字列リテラルの初期化
  strings = malloc(sizeof(String));
  strings->next = NULL;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    error("引数の個数が正しくありません");
    return 1;
  }

  // トークナイズしてパースする
  // 結果はcodeに保存される
  filename = argv[1];
  user_input = read_file(filename);
  init_global_variables();

  token = tokenize();

  program();

  // アセンブリの前半部分を出力
  printf(".intel_syntax noprefix\n\n");

  // グローバル変数の定義
  printf("  .section .data\n\n");
  for (LVar *var = globals; var->next; var = var->next) {
    printf("  .globl %.*s\n", var->len, var->name);
    printf("  .p2align 3\n");
    printf("%.*s:\n", var->len, var->name);
    if (var->offset) {
      printf("  .long %d\n", var->offset);
    } else {
      printf("  .zero %d\n", get_type_size(var->type));
    }
    printf("\n");
  }

  // 文字列リテラル
  for (String *str = strings; str->next; str = str->next) {
    printf("  .section .rodata\n");
    printf(".L.str%d:\n", str->label);
    printf("  .string \"%.*s\"\n", str->len, str->text);
  }

  // 先頭の式から順にコード生成
  printf("  .section .text\n");
  for (int i = 0; code[i]->kind != ND_NONE; i++) {
    gen(code[i]);
  }
  return 0;
}
