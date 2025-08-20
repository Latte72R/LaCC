#include "lacc.h"

extern Token *token_head;
extern Token *token;
extern void *NULL;
extern Function *functions;
extern LVar *locals;
extern LVar *globals;
extern LVar *statics;
extern Object *structs;
extern Object *unions;
extern Object *enums;
extern TypeTag *type_tags;
extern String *strings;
extern String *filenames;
extern Array *arrays;
extern IncludePath *include_paths;

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

static void free_functions(Function *fn) {
  while (fn) {
    Function *next = fn->next;
    free_labels(fn->labels);
    free(fn);
    fn = next;
  }
}

static void free_lvars(LVar *var) {
  while (var) {
    LVar *next = var->next;
    free(var);
    var = next;
  }
}

static void free_objects_list(Object *obj) {
  while (obj) {
    Object *next = obj->next;
    if (obj->var)
      free_lvars(obj->var);
    free(obj);
    obj = next;
  }
}

static void free_type_tags_list(TypeTag *tag) {
  while (tag) {
    TypeTag *next = tag->next;
    free(tag);
    tag = next;
  }
}

static void free_strings_list(String *str) {
  while (str) {
    String *next = str->next;
    free(str);
    str = next;
  }
}

static void free_arrays_list(Array *arr) {
  while (arr) {
    Array *next = arr->next;
    if (arr->val)
      free(arr->val);
    free(arr);
    arr = next;
  }
}

static void free_include_paths_list(IncludePath *path) {
  while (path) {
    IncludePath *next = path->next;
    free(path);
    path = next;
  }
}

void free_all_functions() {
  free_functions(functions);
  functions = NULL;
}

void free_all_lvars() {
  free_lvars(locals);
  free_lvars(globals);
  free_lvars(statics);
  locals = globals = statics = NULL;
}

void free_all_objects() {
  free_objects_list(structs);
  free_objects_list(unions);
  free_objects_list(enums);
  structs = unions = enums = NULL;
}

void free_all_type_tags() {
  free_type_tags_list(type_tags);
  type_tags = NULL;
}

void free_all_strings() {
  free_strings_list(strings);
  strings = NULL;
}

void free_all_filenames() {
  free_strings_list(filenames);
  filenames = NULL;
}

void free_all_arrays() {
  free_arrays_list(arrays);
  arrays = NULL;
}

void free_all_include_paths() {
  free_include_paths_list(include_paths);
  include_paths = NULL;
}
