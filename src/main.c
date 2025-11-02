
#include "lacc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int show_warning;
extern int warning_cnt;
extern char *user_input;
extern CharPtrList *user_input_list;
extern Token *token;
extern Token *token_head;
extern FileName *filenames;
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

static char *duplicate_cstring(const char *src) {
  int len = strlen(src);
  char *copy = malloc(len + 1);
  if (!copy)
    error("memory allocation failed");
  memcpy(copy, src, len + 1);
  return copy;
}

static char *normalize_include_path(const char *path) {
  char *copy = duplicate_cstring(path);
  int len = strlen(copy);
  while (len > 1 && copy[len - 1] == '/') {
    copy[len - 1] = '\0';
    len--;
  }
  return copy;
}

static int include_path_exists(const char *path) {
  for (IncludePath *ip = include_paths; ip; ip = ip->next) {
    if (!strcmp(ip->path, path))
      return TRUE;
  }
  return FALSE;
}

static void add_include_path_front(const char *path) {
  char *normalized = normalize_include_path(path);
  IncludePath *ip = malloc(sizeof *ip);
  if (!ip)
    error("memory allocation failed");
  ip->path = normalized;
  ip->next = include_paths;
  include_paths = ip;
}

static void append_include_path_if_absent(const char *path) {
  char *normalized = normalize_include_path(path);
  if (include_path_exists(normalized)) {
    free(normalized);
    return;
  }
  IncludePath *ip = malloc(sizeof *ip);
  if (!ip)
    error("memory allocation failed");
  ip->path = normalized;
  ip->next = NULL;
  if (!include_paths) {
    include_paths = ip;
    return;
  }
  IncludePath *tail = include_paths;
  while (tail->next)
    tail = tail->next;
  tail->next = ip;
}

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
      add_include_path_front(argv[++i]);
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
      input_file = malloc(sizeof(char) * (strlen(argv[i]) + 1));
      strncpy(input_file, argv[i], strlen(argv[i]) + 1);
      int length = strlen(input_file);
      FileName *filename = malloc(sizeof(FileName));
      filename->name = input_file;
      filename->next = filenames;
      filenames = filename;
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

  append_include_path_if_absent("/usr/lib/gcc/x86_64-linux-gnu/13/include");
  append_include_path_if_absent("/usr/include/x86_64-linux-gnu");
  append_include_path_if_absent("/usr/include");

  preprocess_initialize_builtins();

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
  user_input_list = malloc(sizeof(CharPtrList));
  user_input_list->str = user_input;
  user_input_list->next = NULL;

  tokenize();
  new_token(TK_EOF, NULL, NULL, 0);
  token = token_head;

  program();

  free_user_input_list();

  free_all_tokens();
  free_all_macros();

  generate_assembly();

  free_all_nodes();
  free_all_functions();
  free_all_lvars();
  free_all_objects();
  free_all_type_tags();
  free_all_strings();
  free_all_arrays();
  free_all_include_paths();
  free_all_filenames();
  free_all_types();
  free_all_char_ptrs();

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
