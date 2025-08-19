#include "lacc.h"

extern Token *token_head;
extern Token *token;
extern Node **code;
extern void *NULL;

void free_all_tokens() {
  Token *cur = token_head;
  while (cur) {
    Token *next = cur->next;
    if (cur->loc)
      free(cur->loc);
    free(cur);
    cur = next;
  }
  token_head = NULL;
  token = NULL;
}

void free_node(Node *node) {
  if (!node)
    return;
  if (node->kind == ND_NONE) {
    free(node);
    return;
  }

  switch (node->kind) {
  case ND_ASSIGN:
  case ND_POSTINC:
    if (node->rhs)
      free_node(node->rhs);
    break;
  default:
    if (node->lhs)
      free_node(node->lhs);
    if (node->rhs)
      free_node(node->rhs);
    break;
  }
  if (node->cond)
    free_node(node->cond);
  if (node->then)
    free_node(node->then);
  if (node->els)
    free_node(node->els);
  if (node->init)
    free_node(node->init);
  if (node->step)
    free_node(node->step);
  if (node->body) {
    int i = 0;
    while (node->body[i] && node->body[i]->kind != ND_NONE) {
      free_node(node->body[i]);
      i++;
    }
    if (node->body[i])
      free_node(node->body[i]);
    free(node->body);
  }
  for (int i = 0; i < 6; i++)
    if (node->args[i])
      free_node(node->args[i]);
  if (node->cases)
    free(node->cases);
  free(node);
}

void free_all_nodes() {
  if (!code)
    return;
  int i = 0;
  while (code[i] && code[i]->kind != ND_NONE) {
    free_node(code[i]);
    i++;
  }
  if (code[i])
    free_node(code[i]);
  free(code);
  code = NULL;
}

static TypeList *type_list = 0;

void register_type(Type *type) {
  TypeList *tl = malloc(sizeof(TypeList));
  tl->type = type;
  tl->next = type_list;
  type_list = tl;
}

void free_all_types() {
  TypeList *tl = type_list;
  while (tl) {
    TypeList *next = tl->next;
    free(tl->type);
    free(tl);
    tl = next;
  }
  type_list = NULL;
}

static void free_labels(Label *label) {
  while (label) {
    Label *next = label->next;
    free(label);
    label = next;
  }
}

void free_all_functions(Function *fn) {
  while (fn) {
    Function *next = fn->next;
    free_labels(fn->labels);
    free(fn);
    fn = next;
  }
}

void free_all_lvars(LVar *var) {
  while (var) {
    LVar *next = var->next;
    free(var);
    var = next;
  }
}

void free_all_objects(Object *obj) {
  while (obj) {
    Object *next = obj->next;
    if (obj->var)
      free_all_lvars(obj->var);
    free(obj);
    obj = next;
  }
}

void free_all_type_tags(TypeTag *tag) {
  while (tag) {
    TypeTag *next = tag->next;
    free(tag);
    tag = next;
  }
}

void free_all_strings(String *str) {
  while (str) {
    String *next = str->next;
    free(str);
    str = next;
  }
}

void free_all_arrays(Array *arr) {
  while (arr) {
    Array *next = arr->next;
    if (arr->val)
      free(arr->val);
    free(arr);
    arr = next;
  }
}

void free_all_include_paths(IncludePath *path) {
  while (path) {
    IncludePath *next = path->next;
    free(path);
    path = next;
  }
}
