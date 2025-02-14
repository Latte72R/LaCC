
#include "9cc.h"

char *user_input;
Token *token;
Node *code[100];
int labelseq = 1;
int variable_cnt = 0;
int loop_id = -1;
Function *functions;
Function *current_fn;
LVar *globals;

int main(int argc, char **argv) {
  if (argc != 2) {
    error("引数の個数が正しくありません");
    return 1;
  }

  // トークナイズしてパースする
  // 結果はcodeに保存される
  user_input = argv[1];
  token = tokenize();
  program();

  // アセンブリの前半部分を出力
  printf(".intel_syntax noprefix\n");
  printf(".globl main\n");

  // グローバル変数の定義
  printf(".data\n");
  for (LVar *var = globals; var; var = var->next) {
    printf("%.*s:\n", var->len, var->name);
    printf("  .zero %d\n", get_type_size(var->type));
  }

  // 先頭の式から順にコード生成
  printf(".text\n");
  for (int i = 0; code[i]; i++) {
    gen(code[i]);
  }
  return 0;
}
