// builtin.c
// FORTIFY 系組み込み関数の宣言とフォールバック先登録

#include "diagnostics.h"
#include "runtime.h"
#include "parser.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static Function *find_function_by_name(const char *name) {
  int len = (int)strlen(name);
  for (Function *fn = functions; fn; fn = fn->next) {
    if (fn->len == len && strncmp(fn->name, name, len) == 0)
      return fn;
  }
  return NULL;
}

static Function *create_function_entry(const char *name) {
  Function *fn = malloc(sizeof(Function));
  if (!fn)
    error("memory allocation failed");
  fn->next = functions;
  functions = fn;
  fn->name = (char *)name;
  fn->len = (int)strlen(name);
  fn->offset = 0;
  fn->is_static = false;
  fn->labels = NULL;
  fn->type = NULL;
  fn->type_check = false;
  fn->is_defined = false;
  fn->builtin_kind = BUILTIN_FN_NONE;
  fn->builtin_alias = NULL;
  fn->is_inline = false;
  return fn;
}

static Function *ensure_function_entry(const char *name) {
  Function *fn = find_function_by_name(name);
  if (fn)
    return fn;
  return create_function_entry(name);
}

static void configure_common_fields(Function *fn, Type *type) {
  fn->offset = 0;
  fn->is_static = false;
  fn->labels = NULL;
  fn->type = type;
  fn->is_defined = false;
  fn->type_check = (type->param_count != 0) && !type->is_variadic;
}

static Type *make_size_t_type(void) {
  Type *type = new_type(TY_LONG);
  type->is_unsigned = true;
  return type;
}

static Type *make_int_type(void) {
  return new_type(TY_INT);
}

static Type *make_void_ptr(int is_const) {
  Type *base = new_type(TY_VOID);
  base->is_const = is_const;
  return new_type_ptr(base);
}

static Type *make_char_ptr(int is_const) {
  Type *base = new_type(TY_CHAR);
  base->is_const = is_const;
  return new_type_ptr(base);
}

static Type *make_function_type(Type *return_type, Type **params, int param_count) {
  Type *type = new_type(TY_FUNC);
  type->return_type = return_type;
  type->param_count = param_count;
  type->is_variadic = false;
  for (int i = 0; i < param_count; i++)
    type->param_types[i] = params[i];
  return type;
}

static Function *define_plain_function(const char *name, Type *type) {
  Function *fn = ensure_function_entry(name);
  configure_common_fields(fn, type);
  fn->builtin_kind = BUILTIN_FN_NONE;
  fn->builtin_alias = NULL;
  fn->is_inline = false;
  return fn;
}

static Function *define_builtin_function(const char *name, Type *type, BuiltinFunctionKind kind, Function *alias) {
  Function *fn = ensure_function_entry(name);
  configure_common_fields(fn, type);
  fn->builtin_kind = kind;
  fn->builtin_alias = alias;
  fn->is_inline = false;
  return fn;
}

static Type *make_memcpy_type(void) {
  Type *params[3];
  params[0] = make_void_ptr(false);
  params[1] = make_void_ptr(true);
  params[2] = make_size_t_type();
  return make_function_type(make_void_ptr(false), params, 3);
}

static Type *make_memcpy_chk_type(void) {
  Type *params[4];
  params[0] = make_void_ptr(false);
  params[1] = make_void_ptr(true);
  params[2] = make_size_t_type();
  params[3] = make_size_t_type();
  return make_function_type(make_void_ptr(false), params, 4);
}

static Type *make_memset_type(void) {
  Type *params[3];
  params[0] = make_void_ptr(false);
  params[1] = make_int_type();
  params[2] = make_size_t_type();
  return make_function_type(make_void_ptr(false), params, 3);
}

static Type *make_memset_chk_type(void) {
  Type *params[4];
  params[0] = make_void_ptr(false);
  params[1] = make_int_type();
  params[2] = make_size_t_type();
  params[3] = make_size_t_type();
  return make_function_type(make_void_ptr(false), params, 4);
}

static Type *make_strcpy_type(void) {
  Type *params[2];
  params[0] = make_char_ptr(false);
  params[1] = make_char_ptr(true);
  return make_function_type(make_char_ptr(false), params, 2);
}

static Type *make_strcpy_chk_type(void) {
  Type *params[3];
  params[0] = make_char_ptr(false);
  params[1] = make_char_ptr(true);
  params[2] = make_size_t_type();
  return make_function_type(make_char_ptr(false), params, 3);
}

static Type *make_strncpy_type(void) {
  Type *params[3];
  params[0] = make_char_ptr(false);
  params[1] = make_char_ptr(true);
  params[2] = make_size_t_type();
  return make_function_type(make_char_ptr(false), params, 3);
}

static Type *make_strncpy_chk_type(void) {
  Type *params[4];
  params[0] = make_char_ptr(false);
  params[1] = make_char_ptr(true);
  params[2] = make_size_t_type();
  params[3] = make_size_t_type();
  return make_function_type(make_char_ptr(false), params, 4);
}

static Type *make_stpcpy_type(void) {
  Type *params[2];
  params[0] = make_char_ptr(false);
  params[1] = make_char_ptr(true);
  return make_function_type(make_char_ptr(false), params, 2);
}

static Type *make_stpcpy_chk_type(void) {
  Type *params[3];
  params[0] = make_char_ptr(false);
  params[1] = make_char_ptr(true);
  params[2] = make_size_t_type();
  return make_function_type(make_char_ptr(false), params, 3);
}

static Type *make_memmove_type(void) {
  Type *params[3];
  params[0] = make_void_ptr(false);
  params[1] = make_void_ptr(true);
  params[2] = make_size_t_type();
  return make_function_type(make_void_ptr(false), params, 3);
}

static Type *make_memmove_chk_type(void) {
  Type *params[4];
  params[0] = make_void_ptr(false);
  params[1] = make_void_ptr(true);
  params[2] = make_size_t_type();
  params[3] = make_size_t_type();
  return make_function_type(make_void_ptr(false), params, 4);
}

static Type *make_snprintf_type(void) {
  Type *params[3];
  params[0] = make_char_ptr(false);
  params[1] = make_size_t_type();
  params[2] = make_char_ptr(true);
  Type *type = make_function_type(make_int_type(), params, 3);
  type->is_variadic = true;
  return type;
}

static Type *make_snprintf_chk_type(void) {
  Type *params[5];
  params[0] = make_char_ptr(false);
  params[1] = make_size_t_type();
  params[2] = make_int_type();
  params[3] = make_size_t_type();
  params[4] = make_char_ptr(true);
  Type *type = make_function_type(make_int_type(), params, 5);
  type->is_variadic = true;
  return type;
}

static Type *make_vsnprintf_type(void) {
  Type *params[4];
  params[0] = make_char_ptr(false);
  params[1] = make_size_t_type();
  params[2] = make_char_ptr(true);
  params[3] = make_void_ptr(false);
  return make_function_type(make_int_type(), params, 4);
}

static Type *make_vsnprintf_chk_type(void) {
  Type *params[6];
  params[0] = make_char_ptr(false);
  params[1] = make_size_t_type();
  params[2] = make_int_type();
  params[3] = make_size_t_type();
  params[4] = make_char_ptr(true);
  params[5] = make_void_ptr(false);
  return make_function_type(make_int_type(), params, 6);
}

static Type *make_object_size_type(void) {
  Type *params[2];
  params[0] = make_void_ptr(true);
  params[1] = make_int_type();
  return make_function_type(make_size_t_type(), params, 2);
}

void initialize_builtin_functions(void) {
  Function *memcpy_fn = define_plain_function("memcpy", make_memcpy_type());
  Function *memmove_fn = define_plain_function("memmove", make_memmove_type());
  Function *memset_fn = define_plain_function("memset", make_memset_type());
  Function *strcpy_fn = define_plain_function("strcpy", make_strcpy_type());
  Function *strncpy_fn = define_plain_function("strncpy", make_strncpy_type());
  Function *stpcpy_fn = define_plain_function("stpcpy", make_stpcpy_type());
  Function *snprintf_fn = define_plain_function("snprintf", make_snprintf_type());
  Function *vsnprintf_fn = define_plain_function("vsnprintf", make_vsnprintf_type());

  define_builtin_function("__builtin___memcpy_chk", make_memcpy_chk_type(), BUILTIN_FN_MEMCPY_CHK, memcpy_fn);
  define_builtin_function("__builtin___memmove_chk", make_memmove_chk_type(), BUILTIN_FN_MEMMOVE_CHK, memmove_fn);
  define_builtin_function("__builtin___memset_chk", make_memset_chk_type(), BUILTIN_FN_MEMSET_CHK, memset_fn);
  define_builtin_function("__builtin___strcpy_chk", make_strcpy_chk_type(), BUILTIN_FN_STRCPY_CHK, strcpy_fn);
  define_builtin_function("__builtin___strncpy_chk", make_strncpy_chk_type(), BUILTIN_FN_STRNCPY_CHK, strncpy_fn);
  define_builtin_function("__builtin___stpcpy_chk", make_stpcpy_chk_type(), BUILTIN_FN_STPCPY_CHK, stpcpy_fn);
  define_builtin_function("__builtin___snprintf_chk", make_snprintf_chk_type(), BUILTIN_FN_SNPRINTF_CHK, snprintf_fn);
  define_builtin_function("__builtin___vsnprintf_chk", make_vsnprintf_chk_type(), BUILTIN_FN_VSNPRINTF_CHK, vsnprintf_fn);
  define_builtin_function("__builtin_object_size", make_object_size_type(), BUILTIN_FN_OBJECT_SIZE, NULL);
}

static void drop_trailing_arguments(Node *call, int drop_count) {
  if (drop_count <= 0)
    return;
  if (drop_count > call->val)
    drop_count = call->val;
  int new_count = call->val - drop_count;
  for (int i = new_count; i < call->val; i++)
    call->args[i] = NULL;
  call->val = new_count;
}

static Node *lower_to_alias(Node *call, int drop_count) {
  Function *alias = call->fn->builtin_alias;
  if (!alias)
    return call;
  drop_trailing_arguments(call, drop_count);
  call->fn = alias;
  call->type = alias->type->return_type;
  if (call->lhs && call->lhs->kind == ND_FUNCNAME) {
  	call->lhs->fn = alias;
  	call->lhs->type = new_type_ptr(alias->type);
  }
  return call;
}

static void remove_arguments(Node *call, int index, int count) {
  if (count <= 0 || !call)
    return;
  if (index < 0)
    index = 0;
  if (index >= call->val)
    return;
  if (index + count > call->val)
    count = call->val - index;
  int dst = index;
  int src = index + count;
  while (src < call->val)
    call->args[dst++] = call->args[src++];
  while (dst < call->val)
    call->args[dst++] = NULL;
  call->val -= count;
}

static Node *lower_snprintf_chk(Node *call) {
  if (!call || call->val < 5)
    return call;
  remove_arguments(call, 2, 2);
  return lower_to_alias(call, 0);
}

static Node *lower_vsnprintf_chk(Node *call) {
  if (!call || call->val < 6)
    return call;
  remove_arguments(call, 2, 2);
  return lower_to_alias(call, 0);
}

Node *lower_builtin_function_call(Node *call) {
  if (!call || !call->fn)
    return call;
  switch (call->fn->builtin_kind) {
  case BUILTIN_FN_NONE:
    return call;
  case BUILTIN_FN_OBJECT_SIZE: {
    Node *val = new_num(-1);
    val->type = make_size_t_type();
    return val;
  }
  case BUILTIN_FN_MEMCPY_CHK:
  case BUILTIN_FN_MEMMOVE_CHK:
  case BUILTIN_FN_MEMSET_CHK:
  case BUILTIN_FN_STRCPY_CHK:
  case BUILTIN_FN_STRNCPY_CHK:
  case BUILTIN_FN_STPCPY_CHK:
    return lower_to_alias(call, 1);
  case BUILTIN_FN_SNPRINTF_CHK:
    return lower_snprintf_chk(call);
  case BUILTIN_FN_VSNPRINTF_CHK:
    return lower_vsnprintf_chk(call);
  default:
    return call;
  }
}
