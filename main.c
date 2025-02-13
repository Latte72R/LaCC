
#include "9cc.h"

char *user_input;
Token *token;
Node *code[100];
int labelseq = 1;
int variable_cnt = 0;
int loop_id = -1;
Function *functions;
Function *current_fn;

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

  // 先頭の式から順にコード生成
  for (int i = 0; code[i]; i++) {
    gen(code[i]);
  }
  return 0;
}
