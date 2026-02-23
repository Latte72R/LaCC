
#include "diagnostics.h"
#include "parser.h"
#include "runtime.h"

#include "parser_internal.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static Node *comma_expr();

Node *expr() { return comma_expr(); }

static unsigned long long bit_mask_for_width(int bits) {
  if (bits >= 64)
    return ~0ULL;
  return (1ULL << bits) - 1ULL;
}

static long long sign_extend_width(unsigned long long v, int bits) {
  if (bits >= 64)
    return (long long)v;
  unsigned long long mask = bit_mask_for_width(bits);
  unsigned long long sign = 1ULL << (bits - 1);
  v &= mask;
  if (v & sign)
    v |= ~mask;
  return (long long)v;
}

static Node *build_compare_with_const_fold(NodeKind kind, Node *lhs, Node *rhs) {
  if (lhs && rhs && lhs->kind == ND_NUM && rhs->kind == ND_NUM && lhs->type && rhs->type && is_number(lhs->type) &&
      is_number(rhs->type)) {
    Type *cmp_type = max_type(lhs->type, rhs->type);
    int bits = type_size(cmp_type) * 8;
    if (bits <= 0 || bits > 64)
      bits = 64;
    unsigned long long mask = bit_mask_for_width(bits);
    unsigned long long ul = ((unsigned long long)lhs->val) & mask;
    unsigned long long ur = ((unsigned long long)rhs->val) & mask;
    long long sl = sign_extend_width(ul, bits);
    long long sr = sign_extend_width(ur, bits);

    int v = 0;
    if (cmp_type->is_unsigned) {
      if (kind == ND_EQ)
        v = (ul == ur);
      else if (kind == ND_NE)
        v = (ul != ur);
      else if (kind == ND_LT)
        v = (ul < ur);
      else if (kind == ND_LE)
        v = (ul <= ur);
    } else {
      if (kind == ND_EQ)
        v = (sl == sr);
      else if (kind == ND_NE)
        v = (sl != sr);
      else if (kind == ND_LT)
        v = (sl < sr);
      else if (kind == ND_LE)
        v = (sl <= sr);
    }

    Node *folded = new_num(v ? 1 : 0);
    folded->type = new_type(TY_INT);
    return folded;
  }

  Node *node = new_binary(kind, lhs, rhs);
  node->type = new_type(TY_INT);
  return node;
}

static Node *build_binary_with_const_fold(NodeKind kind, Node *lhs, Node *rhs, Type *type) {
  if (lhs && rhs && type && lhs->kind == ND_NUM && rhs->kind == ND_NUM) {
    int bits = type_size(type) * 8;
    if (bits <= 0 || bits > 64)
      bits = 64;

    unsigned long long mask = bit_mask_for_width(bits);
    unsigned long long ul = ((unsigned long long)lhs->val) & mask;
    unsigned long long ur = ((unsigned long long)rhs->val) & mask;
    long long sl = sign_extend_width(ul, bits);
    long long sr = sign_extend_width(ur, bits);
    int is_unsigned = type->is_unsigned;
    int can_fold = true;
    unsigned long long uv = 0;
    long long sv = 0;
    unsigned int sh = 0;
    if (rhs->val >= 0)
      sh = (unsigned int)rhs->val;

    switch (kind) {
    case ND_ADD:
      uv = (ul + ur) & mask;
      if (!is_unsigned)
        sv = sign_extend_width(uv, bits);
      break;
    case ND_SUB:
      uv = (ul - ur) & mask;
      if (!is_unsigned)
        sv = sign_extend_width(uv, bits);
      break;
    case ND_MUL:
      uv = (ul * ur) & mask;
      if (!is_unsigned)
        sv = sign_extend_width(uv, bits);
      break;
    case ND_DIV:
      if (is_unsigned) {
        if (ur == 0) {
          can_fold = false;
        } else {
          uv = ul / ur;
        }
      } else {
        if (sr == 0) {
          can_fold = false;
        } else {
          long long smin = (bits >= 64) ? LLONG_MIN : -(1LL << (bits - 1));
          if (sl == smin && sr == -1)
            can_fold = false;
          else
            sv = sl / sr;
        }
      }
      break;
    case ND_MOD:
      if (is_unsigned) {
        if (ur == 0) {
          can_fold = false;
        } else {
          uv = ul % ur;
        }
      } else {
        if (sr == 0) {
          can_fold = false;
        } else {
          long long smin = (bits >= 64) ? LLONG_MIN : -(1LL << (bits - 1));
          if (sl == smin && sr == -1)
            can_fold = false;
          else
            sv = sl % sr;
        }
      }
      break;
    case ND_SHL:
      if (rhs->val < 0 || sh >= (unsigned int)bits) {
        can_fold = false;
      } else if (is_unsigned) {
        uv = (ul << sh) & mask;
      } else {
        if (sl < 0) {
          can_fold = false;
        } else if (bits < 64) {
          unsigned long long smax = (1ULL << (bits - 1)) - 1ULL;
          if (ul > (smax >> sh))
            can_fold = false;
          else
            sv = (long long)(ul << sh);
        } else {
          sv = sl << sh;
        }
      }
      break;
    case ND_SHR:
      if (rhs->val < 0 || sh >= (unsigned int)bits) {
        can_fold = false;
      } else if (is_unsigned) {
        uv = ul >> sh;
      } else {
        sv = sl >> sh;
      }
      break;
    case ND_BITAND:
      uv = (ul & ur) & mask;
      if (!is_unsigned)
        sv = sign_extend_width(uv, bits);
      break;
    case ND_BITOR:
      uv = (ul | ur) & mask;
      if (!is_unsigned)
        sv = sign_extend_width(uv, bits);
      break;
    case ND_BITXOR:
      uv = (ul ^ ur) & mask;
      if (!is_unsigned)
        sv = sign_extend_width(uv, bits);
      break;
    default:
      can_fold = false;
      break;
    }

    if (can_fold) {
      Node *folded = new_num((int)(is_unsigned ? uv : sv));
      folded->type = type;
      return folded;
    }
  }

  Node *node = new_binary(kind, lhs, rhs);
  node->type = type;
  return node;
}

static Node *fold_const_cast(Type *type, Node *expr) {
  if (!type || !expr || expr->kind != ND_NUM)
    return NULL;
  if (!(is_number(type) || is_ptr_or_arr(type)))
    return NULL;

  int bits = type_size(type) * 8;
  if (bits <= 0 || bits > 64)
    bits = 64;

  unsigned long long mask = bit_mask_for_width(bits);
  unsigned long long uv = ((unsigned long long)expr->val) & mask;
  long long sv = sign_extend_width(uv, bits);

  Node *folded;
  if (type->ty == TY_BOOL) {
    folded = new_num(expr->val ? 1 : 0);
  } else if (type->is_unsigned || is_ptr_or_arr(type)) {
    folded = new_num((int)uv);
  } else {
    folded = new_num((int)sv);
  }
  folded->type = type;
  return folded;
}

static Node *comma_expr() {
  Node *node = assign();
  while (consume(",")) {
    Node *rhs = assign();
    Node *comma = new_binary(ND_COMMA, node, rhs);
    comma->type = rhs->type;
    comma->endline = rhs->endline;
    node = comma;
  }
  return node;
}

Node *assign_sub(Node *lhs, Node *rhs, Location *loc, int check_const) {
  if (lhs->type->is_const && check_const) {
    error_at(loc, "constant variable cannot be assigned [in assign_sub]");
  } else if (lhs->type->ty == TY_ARR || lhs->type->ty == TY_ARGARR) {
    error_at(loc, "array variable cannot be assigned [in assign_sub]");
  } else if (rhs->type->ty == TY_VOID) {
    error_at(loc, "void type cannot be assigned [in assign_sub]");
  } else if (!is_type_assignable(lhs->type, rhs->type)) {
    error_at(loc, "incompatible operand types ('%s' and '%s') [in assign_sub]", type_name(lhs->type),
             type_name(rhs->type));
  } else if (!is_type_compatible(lhs->type, rhs->type)) {
    // 例外: 整数定数 0 のポインタ代入は互換として扱う
    if (!(is_ptr_or_arr(lhs->type) && rhs->kind == ND_NUM && rhs->val == 0)) {
      if (is_ptr_or_arr(lhs->type) && is_ptr_or_arr(rhs->type)) {
        warning_at(loc, "incompatible operand types ('%s' and '%s') [in assign_sub]", type_name(lhs->type),
                   type_name(rhs->type));
      } else {
        error_at(loc, "incompatible operand types ('%s' and '%s') [in assign_sub]", type_name(lhs->type),
                 type_name(rhs->type));
      }
    }
  }
  Node *node = new_binary(ND_ASSIGN, lhs, rhs);
  return node;
}

Node *assign() {
  Node *node = ternary_operator();
  Node *rhs;
  Location *loc;
  if (consume("=")) {
    loc = consumed_loc;
    // RHS of assignment is an assignment-expression (not comma-expression)
    node = assign_sub(node, assign(), loc, true);
  } else if (consume("+=")) {
    loc = consumed_loc;
    // Use assignment-expression to avoid swallowing comma operator
    rhs = assign();
    if (is_ptr_or_arr(node->type) && is_ptr_or_arr(rhs->type)) {
      error_at(loc, "invalid operands to binary expression ('%s' and '%s') [in assign]", type_name(node->type),
               type_name(rhs->type));
    }
    node = assign_sub(node, new_binary(ND_ADD, node, rhs), loc, true);
  } else if (consume("-=")) {
    loc = consumed_loc;
    rhs = assign();
    if (is_ptr_or_arr(node->type) && is_ptr_or_arr(rhs->type)) {
      warning_at(loc, "incompatible integer to pointer conversion [in assign]");
    }
    node = assign_sub(node, new_binary(ND_SUB, node, rhs), loc, true);
  } else if (consume("*=")) {
    loc = consumed_loc;
    rhs = assign();
    if (is_ptr_or_arr(node->type) && is_ptr_or_arr(rhs->type)) {
      error_at(loc, "invalid operands to binary expression ('%s' and '%s') [in assign]", type_name(node->type),
               type_name(rhs->type));
    }
    node = assign_sub(node, new_binary(ND_MUL, node, rhs), loc, true);
  } else if (consume("/=")) {
    loc = consumed_loc;
    rhs = assign();
    if (is_ptr_or_arr(node->type) && is_ptr_or_arr(rhs->type)) {
      error_at(loc, "invalid operands to binary expression ('%s' and '%s') [in assign]", type_name(node->type),
               type_name(rhs->type));
    }
    node = assign_sub(node, new_binary(ND_DIV, node, rhs), loc, true);
  } else if (consume("%=")) {
    loc = consumed_loc;
    rhs = assign();
    if (is_ptr_or_arr(node->type) && is_ptr_or_arr(rhs->type)) {
      error_at(loc, "invalid operands to binary expression ('%s' and '%s') [in assign]", type_name(node->type),
               type_name(rhs->type));
    }
    node = assign_sub(node, new_binary(ND_MOD, node, rhs), loc, true);
  } else if (consume("&=")) {
    loc = consumed_loc;
    rhs = assign();
    if (!is_number(node->type) || !is_number(rhs->type)) {
      error_at(loc, "invalid operands to binary expression ('%s' and '%s') [in assign]", type_name(node->type),
               type_name(rhs->type));
    }
    node = assign_sub(node, new_binary(ND_BITAND, node, rhs), loc, true);
  } else if (consume("|=")) {
    loc = consumed_loc;
    rhs = assign();
    if (!is_number(node->type) || !is_number(rhs->type)) {
      error_at(loc, "invalid operands to binary expression ('%s' and '%s') [in assign]", type_name(node->type),
               type_name(rhs->type));
    }
    node = assign_sub(node, new_binary(ND_BITOR, node, rhs), loc, true);
  } else if (consume("^=")) {
    loc = consumed_loc;
    rhs = assign();
    if (!is_number(node->type) || !is_number(rhs->type)) {
      error_at(loc, "invalid operands to binary expression ('%s' and '%s') [in assign]", type_name(node->type),
               type_name(rhs->type));
    }
    node = assign_sub(node, new_binary(ND_BITXOR, node, rhs), loc, true);
  } else if (consume("<<=")) {
    loc = consumed_loc;
    rhs = assign();
    if (!is_number(node->type) || !is_number(rhs->type)) {
      error_at(loc, "invalid operands to shift expression ('%s' and '%s') [in assign]", type_name(node->type),
               type_name(rhs->type));
    }
    node = assign_sub(node, new_binary(ND_SHL, node, rhs), loc, true);
  } else if (consume(">>=")) {
    loc = consumed_loc;
    rhs = assign();
    if (!is_number(node->type) || !is_number(rhs->type)) {
      error_at(loc, "invalid operands to shift expression ('%s' and '%s') [in assign]", type_name(node->type),
               type_name(rhs->type));
    }
    node = assign_sub(node, new_binary(ND_SHR, node, rhs), loc, true);
  }
  return node;
}

Node *ternary_operator() {
  Node *node = logical_or();
  if (consume("?")) {
    Node *cond = node;
    // Inside conditional operator, both branches should be parsed as
    // assignment-expressions (not full comma-expressions). Using expr()
    // here would swallow argument separators (',') in contexts like
    // function-call arguments.
    Node *then_branch = assign();
    expect(":", "after then branch", "ternary operator");
    Node *else_branch = assign();
    Node *ternary = new_node(ND_TERNARY);
    ternary->cond = node;
    ternary->then = then_branch;
    ternary->els = else_branch;
    ternary->id = label_cnt++;
    // 型決定:
    // 1) 数値同士: 通常の算術変換（ここでは max_type）
    // 2) ポインタと 0（ヌル定数）: ポインタ型
    // 3) ポインタ同士: 互換なら片側、void* が含まれれば void*、非互換なら警告して then 側
    if (is_number(then_branch->type) && is_number(else_branch->type)) {
      ternary->type = max_type(then_branch->type, else_branch->type);
    } else if (is_ptr_or_arr(then_branch->type) && else_branch->kind == ND_NUM && else_branch->val == 0) {
      ternary->type = then_branch->type;
    } else if (is_ptr_or_arr(else_branch->type) && then_branch->kind == ND_NUM && then_branch->val == 0) {
      ternary->type = else_branch->type;
    } else if (is_ptr_or_arr(then_branch->type) && is_ptr_or_arr(else_branch->type)) {
      Type *lt = then_branch->type->ptr_to;
      Type *rt = else_branch->type->ptr_to;
      if (lt->ty == TY_VOID || rt->ty == TY_VOID) {
        // void* を優先
        Type *t = new_type(TY_PTR);
        t->ptr_to = new_type(TY_VOID);
        ternary->type = t;
      } else if (is_type_compatible(then_branch->type, else_branch->type)) {
        // 互換: 片側の型を採用
        ternary->type = then_branch->type;
      } else {
        warning_at(consumed_loc, "incompatible operand types ('%s' and '%s') [in ternary operator]",
                   type_name(then_branch->type), type_name(else_branch->type));
        ternary->type = then_branch->type;
      }
    } else {
      // それ以外は従来どおり（同一型でなければ警告しつつ then 側の型に合わせる）
      if (!is_type_identical(then_branch->type, else_branch->type)) {
        warning_at(consumed_loc, "incompatible operand types ('%s' and '%s') [in ternary operator]",
                   type_name(then_branch->type), type_name(else_branch->type));
      }
      ternary->type = then_branch->type;
    }

    int ok = true;
    int cond_value = eval_const_expr(cond, &ok);
    if (ok) {
      Node *picked = cond_value ? then_branch : else_branch;
      picked->type = ternary->type;
      node = picked;
    } else {
      node = ternary;
    }
  }
  return node;
}

Node *logical_or() {
  Node *node = logical_and();
  for (;;) {
    if (consume("||")) {
      node = new_binary(ND_OR, node, logical_and());
      // C の規格では論理演算子の結果は常に int 型
      node->type = new_type(TY_INT);
      node->id = label_cnt++;
    } else {
      break;
    }
  }
  return node;
}

Node *logical_and() {
  Node *node = bit_or();
  for (;;) {
    if (consume("&&")) {
      node = new_binary(ND_AND, node, bit_or());
      // C の規格では論理演算子の結果は常に int 型
      node->type = new_type(TY_INT);
      node->id = label_cnt++;
    } else {
      break;
    }
  }
  return node;
}

Node *bit_or() {
  Node *node = bit_xor();
  for (;;) {
    if (consume("|")) {
      Node *rhs = bit_xor();
      Type *type = max_type(node->type, rhs->type);
      node = build_binary_with_const_fold(ND_BITOR, node, rhs, type);
    } else {
      break;
    }
  }
  return node;
}

Node *bit_xor() {
  Node *node = bit_and();
  for (;;) {
    if (consume("^")) {
      Node *rhs = bit_and();
      Type *type = max_type(node->type, rhs->type);
      node = build_binary_with_const_fold(ND_BITXOR, node, rhs, type);
    } else {
      break;
    }
  }
  return node;
}

Node *bit_and() {
  Node *node = equality();
  for (;;) {
    if (consume("&")) {
      Node *rhs = equality();
      Type *type = max_type(node->type, rhs->type);
      node = build_binary_with_const_fold(ND_BITAND, node, rhs, type);
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
      Node *rhs = relational();
      // enum の異種比較は警告
      if (is_enum_type(node->type) && is_enum_type(rhs->type) && node->type->object != rhs->type->object) {
        warning_at(consumed_loc, "comparison of different enum types ('%s' and '%s')", type_name(node->type),
                   type_name(rhs->type));
      }
      // ポインタと整数の比較は警告（ただし整数が 0 のリテラルは除く）
      if ((is_ptr_or_arr(node->type) && is_number(rhs->type) && !(rhs->kind == ND_NUM && rhs->val == 0)) ||
          (is_number(node->type) && is_ptr_or_arr(rhs->type) && !(node->kind == ND_NUM && node->val == 0))) {
        warning_at(consumed_loc, "comparison between pointer and integer ('%s' and '%s')", type_name(node->type),
                   type_name(rhs->type));
      }
      node = build_compare_with_const_fold(ND_EQ, node, rhs);
    } else if (consume("!=")) {
      Node *rhs = relational();
      if (is_enum_type(node->type) && is_enum_type(rhs->type) && node->type->object != rhs->type->object) {
        warning_at(consumed_loc, "comparison of different enum types ('%s' and '%s')", type_name(node->type),
                   type_name(rhs->type));
      }
      if ((is_ptr_or_arr(node->type) && is_number(rhs->type) && !(rhs->kind == ND_NUM && rhs->val == 0)) ||
          (is_number(node->type) && is_ptr_or_arr(rhs->type) && !(node->kind == ND_NUM && node->val == 0))) {
        warning_at(consumed_loc, "comparison between pointer and integer ('%s' and '%s')", type_name(node->type),
                   type_name(rhs->type));
      }
      node = build_compare_with_const_fold(ND_NE, node, rhs);
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
      Node *rhs = bit_shift();
      if (is_enum_type(node->type) && is_enum_type(rhs->type) && node->type->object != rhs->type->object) {
        warning_at(consumed_loc, "comparison of different enum types ('%s' and '%s')", type_name(node->type),
                   type_name(rhs->type));
      }
      if ((is_ptr_or_arr(node->type) && is_number(rhs->type) && !(rhs->kind == ND_NUM && rhs->val == 0)) ||
          (is_number(node->type) && is_ptr_or_arr(rhs->type) && !(node->kind == ND_NUM && node->val == 0))) {
        warning_at(consumed_loc, "comparison between pointer and integer ('%s' and '%s')", type_name(node->type),
                   type_name(rhs->type));
      }
      node = build_compare_with_const_fold(ND_LT, node, rhs);
    } else if (consume("<=")) {
      Node *rhs = bit_shift();
      if (is_enum_type(node->type) && is_enum_type(rhs->type) && node->type->object != rhs->type->object) {
        warning_at(consumed_loc, "comparison of different enum types ('%s' and '%s')", type_name(node->type),
                   type_name(rhs->type));
      }
      if ((is_ptr_or_arr(node->type) && is_number(rhs->type) && !(rhs->kind == ND_NUM && rhs->val == 0)) ||
          (is_number(node->type) && is_ptr_or_arr(rhs->type) && !(node->kind == ND_NUM && node->val == 0))) {
        warning_at(consumed_loc, "comparison between pointer and integer ('%s' and '%s')", type_name(node->type),
                   type_name(rhs->type));
      }
      node = build_compare_with_const_fold(ND_LE, node, rhs);
    } else if (consume(">")) {
      Node *lhs = bit_shift();
      if (is_enum_type(lhs->type) && is_enum_type(node->type) && lhs->type->object != node->type->object) {
        warning_at(consumed_loc, "comparison of different enum types ('%s' and '%s')", type_name(lhs->type),
                   type_name(node->type));
      }
      if ((is_ptr_or_arr(node->type) && is_number(lhs->type) && !(lhs->kind == ND_NUM && lhs->val == 0)) ||
          (is_number(node->type) && is_ptr_or_arr(lhs->type) && !(node->kind == ND_NUM && node->val == 0))) {
        warning_at(consumed_loc, "comparison between pointer and integer ('%s' and '%s')", type_name(node->type),
                   type_name(lhs->type));
      }
      node = build_compare_with_const_fold(ND_LT, lhs, node);
    } else if (consume(">=")) {
      Node *lhs = bit_shift();
      if (is_enum_type(lhs->type) && is_enum_type(node->type) && lhs->type->object != node->type->object) {
        warning_at(consumed_loc, "comparison of different enum types ('%s' and '%s')", type_name(lhs->type),
                   type_name(node->type));
      }
      if ((is_ptr_or_arr(node->type) && is_number(lhs->type) && !(lhs->kind == ND_NUM && lhs->val == 0)) ||
          (is_number(node->type) && is_ptr_or_arr(lhs->type) && !(node->kind == ND_NUM && node->val == 0))) {
        warning_at(consumed_loc, "comparison between pointer and integer ('%s' and '%s')", type_name(node->type),
                   type_name(lhs->type));
      }
      node = build_compare_with_const_fold(ND_LE, lhs, node);
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
      Node *rhs = add();
      node = build_binary_with_const_fold(ND_SHL, node, rhs, node->type);
    } else if (consume(">>")) {
      Node *rhs = add();
      node = build_binary_with_const_fold(ND_SHR, node, rhs, node->type);
    } else {
      break;
    }
  }
  return node;
}

// ポインタ + 整数 または 整数 + ポインタ の場合に
// 整数側を型サイズで乗算するラッパ
Node *new_add(Node *lhs, Node *rhs, Location *loc) {
  Node *node;
  Node *mul_node;
  // lhsがloc, rhsがptrなら
  if (is_ptr_or_arr(lhs->type) && is_ptr_or_arr(rhs->type)) {
    error_at(loc, "invalid operands to binary expression [in new_add]");
  }
  // lhsがloc, rhsがintなら
  else if (is_ptr_or_arr(lhs->type) && is_number(rhs->type)) {
    Type *mul_type = rhs->type;
    mul_node = build_binary_with_const_fold(ND_MUL, rhs, new_num(get_sizeof(lhs->type->ptr_to)), mul_type);
    node = build_binary_with_const_fold(ND_ADD, lhs, mul_node, lhs->type);
  }
  // lhsがint, rhsがptrなら
  else if (is_number(lhs->type) && is_ptr_or_arr(rhs->type)) {
    Type *mul_type = lhs->type;
    mul_node = build_binary_with_const_fold(ND_MUL, lhs, new_num(get_sizeof(rhs->type->ptr_to)), mul_type);
    node = build_binary_with_const_fold(ND_ADD, mul_node, rhs, rhs->type);
  }
  // それ以外は普通に演算
  else {
    node = build_binary_with_const_fold(ND_ADD, lhs, rhs, max_type(lhs->type, rhs->type));
  }
  return node;
}

Node *new_sub(Node *lhs, Node *rhs, Location *loc) {
  Node *node;
  Node *mul_node;

  // lhsがloc, rhsがptrなら
  if (is_ptr_or_arr(lhs->type) && is_ptr_or_arr(rhs->type)) {
    node = new_binary(ND_SUB, lhs, rhs);
    node->type = lhs->type;
    node = new_binary(ND_DIV, node, new_num(get_sizeof(lhs->type->ptr_to)));
    node->type = new_type(TY_INT);
  }
  // lhsがint, rhsがptrなら
  else if (is_number(lhs->type) && is_ptr_or_arr(rhs->type)) {
    error_at(loc, "invalid operands to binary expression [in new_sub]");
  }
  // lhsがloc, rhsがintなら
  else if (is_ptr_or_arr(lhs->type) && is_number(rhs->type)) {
    Type *mul_type = rhs->type;
    mul_node = build_binary_with_const_fold(ND_MUL, rhs, new_num(get_sizeof(lhs->type->ptr_to)), mul_type);
    node = build_binary_with_const_fold(ND_SUB, lhs, mul_node, lhs->type);
  }
  // それ以外は普通に演算
  else {
    node = build_binary_with_const_fold(ND_SUB, lhs, rhs, max_type(lhs->type, rhs->type));
  }
  return node;
}

// add = mul ("+" mul | "-" mul)*
// ポインタ演算を挟み込む
Node *add() {
  Location *consumed_loc_prev;
  Node *node = mul();
  for (;;) {
    if (consume("+")) {
      consumed_loc_prev = consumed_loc;
      node = new_add(node, mul(), consumed_loc_prev);
    } else if (consume("-")) {
      consumed_loc_prev = consumed_loc;
      node = new_sub(node, mul(), consumed_loc_prev);
    } else {
      break;
    }
  }
  return node;
}

Type *resolve_type_mul(Type *left, Type *right, Location *loc) {
  if (is_number(left) && is_number(right)) {
    return max_type(left, right);
  }
  error_at(loc, "invalid operands to binary expression [in resolve_type_mul]");
  return NULL;
}

// mul = unary ("*" unary | "/" unary)*
Node *mul() {
  Location *consumed_loc_prev;
  Node *node = type_cast();

  for (;;) {
    if (consume("*")) {
      consumed_loc_prev = consumed_loc;
      Node *rhs = type_cast();
      Type *type = resolve_type_mul(node->type, rhs->type, consumed_loc_prev);
      node = build_binary_with_const_fold(ND_MUL, node, rhs, type);
    } else if (consume("/")) {
      consumed_loc_prev = consumed_loc;
      Node *rhs = type_cast();
      Type *type = resolve_type_mul(node->type, rhs->type, consumed_loc_prev);
      node = build_binary_with_const_fold(ND_DIV, node, rhs, type);
    } else if (consume("%")) {
      consumed_loc_prev = consumed_loc;
      Node *rhs = type_cast();
      Type *type = resolve_type_mul(node->type, rhs->type, consumed_loc_prev);
      node = build_binary_with_const_fold(ND_MOD, node, rhs, type);
    } else {
      break;
    }
  }
  return node;
}

Node *type_cast() {
  Token *tok = token;
  if (!consume("(")) {
    token = tok;
    return unary();
  }
  Type *type = consume_type(true);
  if (!type) {
    token = tok;
    return unary();
  }
  if (!consume(")")) {
    error_at(token->loc, "expected ')' after type cast [in type_cast]");
  }
  Node *node = new_node(ND_TYPECAST);
  // cast-expression is right-associative: (T) cast-expression
  node->lhs = type_cast();
  node->type = type;
  if (type->ty == TY_VOID) {
    // void型へのキャストは評価のみ
    return node->lhs;
  } else if (type->ty == TY_VOID) {
    error_at(tok->loc, "cannot cast to void type [in type_cast]");
  } else if (type->ty == TY_STRUCT) {
    error_at(tok->loc, "cannot cast to struct type [in type_cast]");
  } else if (type->ty == TY_UNION) {
    error_at(tok->loc, "cannot cast to union type [in type_cast]");
  } else if (node->lhs->type->ty == TY_STRUCT) {
    error_at(tok->loc, "cannot cast from struct type [in type_cast]");
  } else if (node->lhs->type->ty == TY_UNION) {
    error_at(tok->loc, "cannot cast from union type [in type_cast]");
  }

  // 警告: ポインタ → より小さい整数型へのキャスト
  if (is_ptr_or_arr(node->lhs->type) && is_number(type)) {
    int from_sz = type_size(node->lhs->type); // ポインタ幅 (LP64 なら 8)
    int to_sz = type_size(type);
    if (to_sz < from_sz) {
      warning_at(tok->loc, "cast to smaller integer type '%s' from pointer [in type cast]", type_name(type));
    }
  }

  // (char)123 のような定数キャストは frontend で即値化
  Node *folded = fold_const_cast(type, node->lhs);
  if (folded)
    return folded;

  return node;
}

// unary = ("+" | "-")? unary
//       | primary
Node *unary() {
  Node *node;
  if (consume("sizeof")) {
    // sizeof は 2 形態: sizeof unary-expression | sizeof ( type-name )
    int sz;
    if (consume("(")) {
      Token *tok2 = token;
      Type *ty = consume_type(true);
      if (ty) {
        expect(")", "after type", "sizeof");
        sz = get_sizeof(ty);
      } else {
        // 型ではなかったので式として解釈
        token = tok2;
        Node *e = expr();
        expect(")", "after expression", "sizeof");
        sz = get_sizeof(e->type);
      }
    } else {
      sz = get_sizeof(unary()->type);
    }
    Node *n = new_num(sz);
    // C の sizeof の結果型は size_t（LP64想定で unsigned long）
    n->type = new_type(TY_LONG);
    n->type->is_unsigned = true;
    return n;
  }
  if (consume("+"))
    return type_cast();
  if (consume("-")) {
    Node *rhs = type_cast();
    int ok = true;
    int value = eval_const_expr(rhs, &ok);
    if (ok) {
      Node *folded = new_num(-value);
      folded->type = rhs->type;
      return folded;
    }
    node = new_binary(ND_SUB, new_num(0), rhs);
    node->type = node->rhs->type;
    return node;
  }
  if (consume("&")) {
    node = new_node(ND_ADDR);
    node->lhs = type_cast();
    node->type = new_type_ptr(node->lhs->type);
    return node;
  }
  if (consume("*")) {
    Location *consumed_loc_prev = consumed_loc;
    node = type_cast();
    node = new_deref(node);
    if (!is_ptr_or_arr(node->lhs->type)) {
      error_at(consumed_loc_prev, "invalid pointer dereference");
    }
    return node;
  }
  if (consume("!")) {
    node = new_node(ND_NOT);
    node->lhs = type_cast();
    // 論理否定の結果は常に int 型
    node->type = new_type(TY_INT);
    return node;
  }
  if (consume("~")) {
    node = new_node(ND_BITNOT);
    node->lhs = type_cast();
    if (!is_number(node->lhs->type))
      error_at(consumed_loc, "invalid operand to unary '~'");
    node->type = max_type(node->lhs->type, new_type(TY_INT));
    return node;
  }
  return increment_decrement();
}

Node *increment_decrement() {
  Node *node;
  Location *loc;
  if (consume("++")) {
    loc = consumed_loc;
    node = access_member();
    return assign_sub(node, new_add(node, new_num(1), consumed_loc), loc, true);
  } else if (consume("--")) {
    loc = consumed_loc;
    node = access_member();
    return assign_sub(node, new_sub(node, new_num(1), consumed_loc), loc, true);
  }
  node = access_member();
  if (consume("++")) {
    node = new_binary(ND_POSTINC, node, new_add(node, new_num(1), consumed_loc));
  } else if (consume("--")) {
    node = new_binary(ND_POSTINC, node, new_sub(node, new_num(1), consumed_loc));
  }
  return node;
}

// Objecture Reference and Array Indexing
Node *access_member() {
  Node *ptr_node;
  Node *offset_node;
  Token *tok;
  Object *object;
  LVar *var;
  Location *consumed_loc_prev;
  Token *prev_tok = token;
  Node *node = primary();
  for (;;) {
    if (consume("[")) {
      consumed_loc_prev = consumed_loc;
      if (!is_ptr_or_arr(node->type)) {
        error_at(consumed_loc_prev, "invalid array access [in primary]");
      }
      node = new_add(node, expr(), consumed_loc_prev);
      expect("]", "after number", "array access");
      node = new_deref(node);
    } else if (consume("(")) {
      Location *loc = consumed_loc;
      if (node->kind == ND_DEREF && node->lhs->type->ty == TY_PTR && node->lhs->type->ptr_to->ty == TY_FUNC)
        node = node->lhs;
      Node *call = new_node(ND_FUNCALL);
      call->lhs = node;
      call->id = label_cnt++;
      if (node->kind == ND_FUNCNAME)
        call->fn = node->fn;
      Type *ftype;
      if (node->kind == ND_FUNCNAME) {
        ftype = node->fn->type;
      } else if (node->type->ty == TY_PTR && node->type->ptr_to->ty == TY_FUNC) {
        ftype = node->type->ptr_to;
      } else if (node->type->ty == TY_FUNC) {
        ftype = node->type;
      } else {
        error_at(loc, "not a function or function pointer [in function call]");
      }
      call->type = ftype->return_type;
      if (!consume(")")) {
        int n = 0;
        for (;;) {
          if (n >= MAX_FUNC_PARAMS) {
            error_at(loc, "too many arguments to function call (supports up to %d)", MAX_FUNC_PARAMS);
          }
          call->args[n] = assign();
          n += 1;
          if (!consume(","))
            break;
        }
        call->val = n;
        expect(")", "after arguments", "function call");
      } else {
        call->val = 0;
      }
      int check;
      if (call->fn)
        check = call->fn->type_check;
      else
        check = !ftype->is_variadic;
      if (check) {
        if (call->val > ftype->param_count) {
          error_at(loc, "too many arguments to function call");
        } else if (call->val < ftype->param_count) {
          error_at(loc, "not enough arguments to function call");
        } else {
          for (int i = 0; i < call->val; i++) {
            if (!is_type_compatible(ftype->param_types[i], call->args[i]->type)) {
              warning_at(loc, "incompatible %s to %s conversion [in function call]", type_name(call->args[i]->type),
                         type_name(ftype->param_types[i]));
              break;
            }
          }
        }
      }
      if (call->fn) {
        node = lower_builtin_function_call(call);
      } else {
        node = call;
      }
    } else if (consume(".")) {
      if (node->type->ty != TY_STRUCT && node->type->ty != TY_UNION) {
        error_at(prev_tok->loc, "%.*s is not an object [in object reference]", prev_tok->len, prev_tok->str);
      }
      object = node->type->object;
      if (!object) {
        error_at(prev_tok->loc, "unknown object: %.*s [in object reference]", prev_tok->len, prev_tok->str);
      } else if (!object->is_defined) {
        error_at(prev_tok->loc, "incomplete definition of type [in object reference]");
      }
      tok = expect_ident("object reference");
      var = find_object_member(object, tok);
      offset_node = new_num(var->offset);
      ptr_node = new_node(ND_ADDR);
      ptr_node->lhs = node;
      ptr_node->type = new_type_ptr(node->type);
      ptr_node = new_binary(ND_ADD, ptr_node, offset_node);
      ptr_node->type = new_type_ptr(var->type);
      node = new_deref(ptr_node);
    } else if (consume("->")) {
      if (node->type->ty != TY_PTR) {
        error_at(prev_tok->loc, "%.*s is not a pointer [in struct reference]", prev_tok->len, prev_tok->str);
      }
      if (node->type->ptr_to->ty != TY_STRUCT && node->type->ptr_to->ty != TY_UNION) {
        error_at(prev_tok->loc, "%.*s is not a pointer of object [in struct reference]", prev_tok->len, prev_tok->str);
      }
      object = node->type->ptr_to->object;
      if (!object) {
        error_at(prev_tok->loc, "unknown object: %.*s [in object reference]", prev_tok->len, prev_tok->str);
      } else if (!object->is_defined) {
        error_at(prev_tok->loc, "incomplete definition of type [in object reference]", prev_tok->len, prev_tok->str);
      }
      tok = expect_ident("object reference");
      var = find_object_member(object, tok);
      offset_node = new_num(var->offset);
      ptr_node = new_binary(ND_ADD, node, offset_node);
      ptr_node->type = new_type_ptr(var->type);
      node = new_deref(ptr_node);
    } else
      break;
  }
  return node;
}

// primary = "(" expr ")" | num
Node *primary() {
  Node *node;
  Token *tok;

  // 括弧
  if (consume("(")) {
    // GNU statement expression: ({ ... })
    if (consume("{")) {
      // Parse a compound statement as an expression and take the value of the
      // last expression statement inside the block.
      Node *blk = block_stmt(); // assumes '{' already consumed
      expect(")", "after statement expression body", "statement expression");

      // Determine the last non-ND_NONE statement in the block
      int last = -1;
      for (int i = 0; blk->body[i]->kind != ND_NONE; i++)
        last = i;
      if (last < 0) {
        error_at(consumed_loc, "empty statement expression [in primary]");
      }
      Node *last_stmt = blk->body[last];
      // Ensure the last statement leaves a value on the stack when used as an expression.
      // If it's an expression statement, it has endline=true; flip it so gen() keeps the value.
      // If it's not an expression, we still allow it but set type to int.
      last_stmt->endline = false;

      Node *se = new_node(ND_STMTEXPR);
      se->then = blk; // reuse 'then' to point to body block
      se->type = last_stmt->type ? last_stmt->type : new_type(TY_INT);
      return se;
    }
    node = expr();
    expect(")", "after expression", "primary");
    return node;
  }

  // 数値
  if (token->kind == TK_NUM) {
    Token *numtok = token;
    consumed_loc = numtok->loc;
    token = token->next;
    Node *n = new_num(numtok->val);
    // Override type based on literal suffix
    Type *t;
    if (numtok->lit_rank == 2)
      t = new_type(TY_LONGLONG);
    else if (numtok->lit_rank == 1)
      t = new_type(TY_LONG);
    else
      t = new_type(TY_INT);
    t->is_unsigned = numtok->lit_is_unsigned;
    n->type = t;
    return n;
  }

  // 文字列
  if (token->kind == TK_STRING) {
    String *str = string_literal();
    node = new_node(ND_STRING);
    node->id = str->id;
    node->type = new_type_ptr(new_type(TY_CHAR));
    return node;
  }

  // 型
  if (is_type(token)) {
    Type *type = consume_type(true);
    node = new_node(ND_TYPE);
    node->type = type;
    return node;
  }

  if (token->kind != TK_IDENT) {
    error_at(token->loc, "expected expression [in primary]");
  }
  tok = expect_ident("primary");

  // Support C99 __func__: expand to a string literal of the current function name
  if (tok->len == 8 && !strncmp(tok->str, "__func__", 8)) {
    if (!current_fn) {
      error_at(tok->loc, "__func__ used outside of function [in primary]");
    }
    String *s = malloc(sizeof(String));
    s->text = current_fn->name;
    s->len = current_fn->len;
    s->id = label_cnt++;
    s->next = strings;
    strings = s;
    node = new_node(ND_STRING);
    node->id = s->id;
    node->type = new_type_ptr(new_type(TY_CHAR));
    return node;
  }

  // enumのメンバー
  LVar *member = find_enum_member(tok);
  if (member) {
    node = new_num(member->offset);
    return node;
  }

  // 変数または関数名
  LVar *lvar = find_lvar(tok);
  LVar *gvar = find_gvar(tok);
  if (lvar) {
    node = new_node(ND_LVAR);
    node->var = lvar;
    node->type = lvar->type;
  } else if (gvar) {
    node = new_node(ND_GVAR);
    node->var = gvar;
    node->type = gvar->type;
  } else {
    Function *fn = find_fn(tok);
    if (fn) {
      node = new_node(ND_FUNCNAME);
      node->fn = fn;
      node->type = new_type_ptr(fn->type);
    } else {
      error_at(tok->loc, "undefined variable: %.*s [in primary]", tok->len, tok->str);
    }
  }
  return node;
}

int compile_time_number() {
  int result;
  if (consume("(")) {
    result = compile_time_number();
    expect(")", "after expression", "compile time number");
  } else if (token->kind == TK_IDENT) {
    LVar *member = find_enum_member(consume_ident());
    if (!member) {
      error_at(token->loc, "expected a compile time constant [in compile time number]");
    }
    result = member->offset;
  } else {
    result = expect_signed_number();
  }
  return result;
}
