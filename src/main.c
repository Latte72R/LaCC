
#include "codegen.h"
#include "diagnostics.h"
#include "lexer.h"
#include "parser.h"
#include "runtime.h"
#include "source.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

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
      return true;
  }
  return false;
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

static void rstrip_whitespace(char *s) {
  size_t len = strlen(s);
  while (len > 0 && isspace((unsigned char)s[len - 1])) {
    s[--len] = '\0';
  }
}

static char *lstrip_whitespace(char *s) {
  while (*s && isspace((unsigned char)*s))
    s++;
  return s;
}

// fd を 1 行ずつ読む（\n を含めない）。0: EOF, 1: 1行読めた, -1: エラー
static int fd_readline(int fd, char *buf, size_t cap) {
  if (cap == 0)
    return -1;

  size_t i = 0;
  while (i + 1 < cap) {
    char c;
    ssize_t n = read(fd, &c, 1);
    if (n == 0) {
      if (i == 0)
        return 0; // EOF
      break;      // 最終行（改行なし）
    }
    if (n < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    if (c == '\n')
      break;
    buf[i++] = c;
  }
  buf[i] = '\0';
  return 1;
}

static void add_include_paths_from_compiler(void) {
  char *argv[] = {(char *)"cc", (char *)"-E", (char *)"-v", (char *)"-xc", (char *)"-", NULL};

  int pipefd[2];
  if (pipe(pipefd) != 0) {
    error("pipe failed: %s", strerror(errno));
  }

  pid_t pid = fork();
  if (pid < 0) {
    error("fork failed: %s", strerror(errno));
  }

  if (pid == 0) {
    // child
    // stderr -> pipe write end
    close(pipefd[0]);
    if (dup2(pipefd[1], STDERR_FILENO) < 0)
      _exit(127);
    close(pipefd[1]);

    // stdin -> /dev/null
    int devnull = open("/dev/null", O_RDONLY);
    if (devnull < 0)
      _exit(127);
    if (dup2(devnull, STDIN_FILENO) < 0)
      _exit(127);
    close(devnull);

    // stdout -> /dev/null
    int devnull2 = open("/dev/null", O_WRONLY);
    if (devnull2 < 0)
      _exit(127);
    if (dup2(devnull2, STDOUT_FILENO) < 0)
      _exit(127);

    execvp(argv[0], argv);
    close(devnull2);
    _exit(127);
  }

  // parent
  close(pipefd[1]);

  char buf[4096];
  int in_block = 0;
  int added = 0;

  while (1) {
    int r = fd_readline(pipefd[0], buf, sizeof buf);
    if (r == 0)
      break;
    if (r < 0) {
      close(pipefd[0]);
      error("read from compiler output failed: %s", strerror(errno));
    }

    if (!in_block) {
      if (strstr(buf, "#include <...> search starts here:"))
        in_block = 1;
      continue;
    }

    if (strstr(buf, "End of search list."))
      break;

    char *line = lstrip_whitespace(buf);
    if (*line == '\0')
      continue;

    char *tag = strstr(line, " (framework directory)");
    if (tag)
      *tag = '\0';

    rstrip_whitespace(line);
    if (*line == '\0')
      continue;

    append_include_path_if_absent(line);
    added = 1;
  }

  close(pipefd[0]);

  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    error("waitpid failed: %s", strerror(errno));
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    // cc が無い/壊れてる/特殊環境 など
    error("cc failed while probing include paths (exit=%d)", WIFEXITED(status) ? WEXITSTATUS(status) : -1);
  }
  if (!added) {
    error("failed to parse include paths from compiler output.");
  }
}

int main(int argc, char **argv) {
  init_global_variables();
  input_file = NULL;
  output_file = NULL;
  int output_file_specified = false;

  if (argc < 2) {
    error("invalid number of arguments.");
  }

  for (int i = 1; i < argc; i++) {
    if (!strncmp(argv[i], "-I", 2) && i + 1 < argc) {
      add_include_path_front(argv[++i]);
    } else if (!strncmp(argv[i], "-S", 2)) {
    } else if (!strncmp(argv[i], "-w", 2)) {
      show_warning = false;
    } else if (!strncmp(argv[i], "-H", 2)) {
      print_include_files = true;
    } else if (!strncmp(argv[i], "-o", 2) && i + 1 < argc) {
      if (output_file_specified) {
        error("multiple output files specified.");
      }
      if (output_file) {
        free(output_file);
      }
      output_file_specified = true;
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
      hierarchy_level = 0;
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

  add_include_paths_from_compiler();
  preprocess_initialize_builtins();
  initialize_builtin_functions();

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

  push_input_context(user_input, input_file, 0);
  tokenize();
  pop_input_context();
  new_token(TK_EOF, NULL, NULL, 0);
  token = token_head;

  program();

  free_user_input_list();

  free_all_tokens();
  free_all_macros();
  free_all_locations();

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
