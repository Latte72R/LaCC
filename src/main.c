
#include "lacc.h"

extern int show_warning;
extern int warning_cnt;
extern char *user_input;
extern Token *token;
extern Token *token_head;
extern String *filenames;
extern char *input_file;
extern char *output_file;
extern IncludePath *include_paths;
extern FILE *fp;
extern const int TRUE;
extern const int FALSE;
extern void *NULL;

int main(int argc, char **argv) {
  init_global_variables();
  input_file = NULL;
  char *output_file_tmp = NULL;

  if (argc < 2) {
    error("invalid number of arguments.");
  }

  for (int i = 1; i < argc; i++) {
    if (!strncmp(argv[i], "-I", 2) && i + 1 < argc) {
      IncludePath *ip = malloc(sizeof *ip);
      ip->path = argv[++i]; // "./include" など
      ip->next = include_paths;
      include_paths = ip;
    } else if (!strncmp(argv[i], "-S", 2)) {
    } else if (!strncmp(argv[i], "-w", 2)) {
      show_warning = FALSE;
    } else if (!strncmp(argv[i], "-o", 2) && i + 1 < argc) {
      output_file_tmp = argv[++i];
      if (output_file_tmp[0] == '-') {
        error("output file cannot start with '-'.");
      }
      output_file = output_file_tmp;
    } else if (argv[i][0] == '-') {
      error("unknown option: %s", argv[i]);
    } else {
      if (input_file) {
        error("multiple source files specified: %s and %s", input_file, argv[i]);
      }
      input_file = argv[i];
      int length = strlen(input_file);
      filenames = malloc(sizeof(String));
      filenames->text = input_file;
      filenames->len = length;
      filenames->next = NULL;
      if (strncmp(input_file + length - 2, ".c", 2)) {
        error("source file must have a .c extension.");
      }
      int start = 0;
      for (int j = length - 2; j >= 0; j--) {
        if (input_file[j] == '/') {
          start = j + 1;
          break;
        }
      }
      output_file_tmp = malloc(length - start + 1);
      strncpy(output_file_tmp, input_file + start, length - start);
      output_file_tmp[length - start - 1] = 's';
      output_file_tmp[length - start] = '\0';
    }
  }

  // トークナイズしてパースする
  // 結果はcodeに保存される
  if (!input_file) {
    error("no source file specified.");
  }

  if (!output_file) {
    output_file = output_file_tmp;
  }
  fp = fopen(output_file, "w");
  if (!fp) {
    error("failed to open output file: %s", output_file);
  }

  user_input = read_file(input_file);

  tokenize();
  new_token(TK_EOF, NULL, NULL, 0);
  token = token_head;

  program();

  generate_assembly();

  fclose(fp);

  if (show_warning) {
    if (warning_cnt == 1) {
      warning("%d warning generated.", warning_cnt);
    } else if (warning_cnt > 1) {
      warning("%d warnings generated.", warning_cnt);
    }
  }

  return 0;
}
