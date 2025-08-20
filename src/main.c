
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
extern Function *functions;
extern LVar *globals;
extern LVar *statics;
extern LVar *locals;
extern Object *structs;
extern Object *unions;
extern Object *enums;
extern TypeTag *type_tags;
extern String *strings;
extern Array *arrays;
extern const int TRUE;
extern const int FALSE;
extern void *NULL;

int main(int argc, char **argv) {
  init_global_variables();
  input_file = NULL;
  output_file = NULL;
  int output_file_specified = FALSE;

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
      if (output_file_specified) {
        error("multiple output files specified.");
      }
      if (output_file) {
        free(output_file);
      }
      output_file_specified = TRUE;
      char *out = argv[++i];
      if (out[0] == '-') {
        error("output file cannot start with '-'.");
      }
      int out_len = strlen(out);
      output_file = malloc(out_len + 1);
      strncpy(output_file, out, out_len + 1);
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
      if (!output_file_specified) {
        output_file = malloc(length - start + 1);
        strncpy(output_file, input_file + start, length - start);
        output_file[length - start - 1] = 's';
        output_file[length - start] = '\0';
      }
    }
  }

  // トークナイズしてパースする
  // 結果はcodeに保存される
  if (!input_file) {
    error("no source file specified.");
  }

  fp = fopen(output_file, "w");
  if (!fp) {
    error("failed to open output file: %s", output_file);
  }

  user_input = read_file(input_file);
  if (!user_input) {
    error("failed to read source file: %s", input_file);
  }

  tokenize();
  new_token(TK_EOF, NULL, NULL, 0);
  token = token_head;

  program();

  free_all_tokens();

  generate_assembly();
  free_all_functions();
  free_all_lvars();
  free_all_objects();
  free_all_type_tags();
  free_all_strings();
  free_all_arrays();
  free_all_include_paths();
  free_all_filenames();
  free(user_input);
  free_all_types();

  fclose(fp);
  free(output_file);

  if (show_warning) {
    if (warning_cnt == 1) {
      warning("%d warning generated.", warning_cnt);
    } else if (warning_cnt > 1) {
      warning("%d warnings generated.", warning_cnt);
    }
  }

  return 0;
}
