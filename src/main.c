
#include "lacc.h"

extern char *user_input;
extern Token *token;
extern String *filenames;
extern char *input_file;
extern char *output_file;
extern IncludePath *include_paths;
extern FILE *fp;
extern void *NULL;

int main(int argc, char **argv) {
  init_global_variables();
  Token *token_cpy;
  input_file = NULL;
  char *output_file_tmp = NULL;

  if (argc < 2) {
    error("Invalid number of arguments.");
  }

  for (int i = 1; i < argc; i++) {
    if (!memcmp(argv[i], "-I", 2) && i + 1 < argc) {
      IncludePath *ip = malloc(sizeof *ip);
      ip->path = argv[++i]; // "./include" など
      ip->next = include_paths;
      include_paths = ip;
    } else if (!memcmp(argv[i], "-S", 2)) {
    } else if (!memcmp(argv[i], "-o", 2) && i + 1 < argc) {
      output_file_tmp = argv[++i];
      if (output_file_tmp[0] == '-') {
        error("Output file cannot start with '-'.");
      }
      output_file = output_file_tmp;
    } else if (argv[i][0] == '-') {
      error("Unknown option: %s", argv[i]);
    } else {
      token_cpy = token;
      input_file = argv[i];
      filenames = malloc(sizeof(String));
      filenames->text = input_file;
      filenames->len = strlen(input_file);
      filenames->next = NULL;
      if (memcmp(input_file + strlen(input_file) - 2, ".c", 2)) {
        error("Source file must have a .c extension.");
      }
      output_file_tmp = malloc(strlen(input_file));
      memcpy(output_file_tmp, input_file, strlen(input_file));
      output_file_tmp[strlen(input_file) - 1] = 's';
    }
  }

  // トークナイズしてパースする
  // 結果はcodeに保存される
  if (!input_file) {
    error("No source file specified.");
  }

  if (!output_file) {
    output_file = output_file_tmp;
  }
  fp = fopen(output_file, "w");
  if (!fp) {
    error("Failed to open output file: %s", output_file);
  }

  user_input = read_file(input_file);

  tokenize();
  new_token(TK_EOF, NULL, 0);
  token = token_cpy->next;

  program();

  generate_assembly();

  fclose(fp);
  return 0;
}
