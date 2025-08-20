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
extern Node **code;

void free_all_tokens() {
  Token *cur = token_head;
  while (cur) {
    Token *next = cur->next;
    free(cur->loc);
    free(cur);
    cur = next;
  }
  token_head = NULL;
  token = NULL;
}

static NodeList *node_list = 0;

void register_node(Node *node) {
  NodeList *nl = malloc(sizeof(NodeList));
  nl->node = node;
  nl->next = node_list;
  node_list = nl;
}

void free_all_nodes() {
  NodeList *nl = node_list;
  while (nl) {
    NodeList *next = nl->next;
    free(nl->node->body);
    free(nl->node->cases);
    free(nl->node);
    free(nl);
    nl = next;
  }
  node_list = NULL;
}

static CharPtrList *char_ptr_list = 0;

void register_char_ptr(char *str) {
  CharPtrList *cl = malloc(sizeof(CharPtrList));
  cl->str = str;
  cl->next = char_ptr_list;
  char_ptr_list = cl;
}

void free_all_char_ptrs() {
  CharPtrList *cl = char_ptr_list;
  while (cl) {
    CharPtrList *next = cl->next;
    free(cl->str);
    free(cl);
    cl = next;
  }
  char_ptr_list = NULL;
}

static LVarList *lvar_list = 0;

void register_lvar(LVar *var) {
  LVarList *ll = malloc(sizeof(LVarList));
  ll->var = var;
  ll->next = lvar_list;
  lvar_list = ll;
}

void free_all_lvars() {
  LVarList *ll = lvar_list;
  while (ll) {
    LVarList *next = ll->next;
    free(ll->var);
    free(ll);
    ll = next;
  }
  lvar_list = NULL;
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

static void free_objects_list(Object *obj) {
  while (obj) {
    Object *next = obj->next;
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
