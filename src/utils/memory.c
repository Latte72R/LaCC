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
