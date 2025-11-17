
#include "diagnostics.h"
#include "runtime.h"
#include "source.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 指定されたファイルの内容を返す
char *read_file(char *path) {
  // ファイルを開く。開けなければ NULL を返す
  FILE *file = fopen(path, "r");
  if (!file)
    return NULL;

  // いったん末尾までシークしてファイルサイズを取得
  if (fseek(file, 0, SEEK_END) == -1)
    error("%s: fseek", path);
  int size = ftell(file);
  if (fseek(file, 0, SEEK_SET) == -1)
    error("%s: fseek", path);

  // サイズ+終端記号分のバッファを確保して読み込み
  char *buf = malloc(size + 2);
  fread(buf, size, 1, file);

  // 読み込んだ内容の末尾に改行とヌル終端を追加
  if (size == 0 || buf[size - 1] != '\n')
    buf[size++] = '\n';
  buf[size] = '\0';
  fclose(file); // ファイルを閉じる
  return buf;
}

static char *duplicate_cstring(const char *src) {
  int len = strlen(src);
  char *copy = malloc(len + 1);
  if (!copy)
    error("memory allocation failed");
  memcpy(copy, src, len + 1);
  return copy;
}

static char *join_paths(const char *dir, const char *name) {
  if (!dir || !dir[0])
    return duplicate_cstring(name);
  int need_sep = dir[strlen(dir) - 1] == '/' ? 0 : 1;
  int len = strlen(dir) + need_sep + strlen(name) + 1;
  char *full = malloc(len);
  if (!full)
    error("memory allocation failed");
  if (need_sep)
    snprintf(full, len, "%s/%s", dir, name);
  else
    snprintf(full, len, "%s%s", dir, name);
  return full;
}

static char *try_read_path(const char *path, char **resolved_name) {
  char *src = read_file((char *)path);
  if (src && resolved_name) {
    *resolved_name = duplicate_cstring(path);
  }
  return src;
}

char *read_include_file(char *name, const char *including_file, int is_system, char **resolved_name) {
  if (resolved_name)
    *resolved_name = NULL;

  char *src = NULL;

  if (!is_system && including_file && name[0] != '\0' && name[0] != '/') {
    const char *slash = strrchr(including_file, '/');
    if (slash) {
      int dir_len = slash - including_file;
      char *dir;
      if (dir_len <= 0) {
        dir = duplicate_cstring("/");
      } else {
        dir = malloc(dir_len + 1);
        if (!dir)
          error("memory allocation failed");
        memcpy(dir, including_file, dir_len);
        dir[dir_len] = '\0';
      }
      char *full = join_paths(dir, name);
      free(dir);
      src = try_read_path(full, resolved_name);
      free(full);
      if (src)
        return src;
    }
  }

  src = try_read_path(name, resolved_name);
  if (src)
    return src;

  for (IncludePath *path = include_paths; path; path = path->next) {
    char *full = join_paths(path->path, name);
    src = try_read_path(full, resolved_name);
    free(full);
    if (src)
      return src;
  }
  return NULL;
}

static int path_is_prefix(const char *prefix, const char *full) {
  int plen = (int)strlen(prefix);
  int flen = (int)strlen(full);
  if (plen == 0 || plen > flen)
    return 0;
  if (strncmp(prefix, full, plen) != 0)
    return 0;
  if (plen == flen)
    return 1;
  return full[plen] == '/';
}

static char *dirname_of(const char *path) {
  const char *slash = strrchr(path, '/');
  if (!slash) {
    return duplicate_cstring("");
  }
  int len = (int)(slash - path);
  if (len <= 0)
    return duplicate_cstring("/");
  char *dir = malloc(len + 1);
  if (!dir)
    error("memory allocation failed");
  memcpy(dir, path, len);
  dir[len] = '\0';
  return dir;
}

// Read file for #include_next: search continue after the directory where including_file was found.
char *read_include_next_file(char *name, const char *including_file, char **resolved_name) {
  if (resolved_name)
    *resolved_name = NULL;

  // Absolute path: try directly
  if (name && name[0] == '/') {
    return try_read_path(name, resolved_name);
  }

  // Determine where the current file came from in include_paths
  IncludePath *start = include_paths;
  if (including_file) {
    // Prefer matching against include path prefixes of the full including file path
    IncludePath *best = NULL;
    for (IncludePath *p = include_paths; p; p = p->next) {
      if (path_is_prefix(p->path, including_file)) {
        if (!best || (int)strlen(p->path) > (int)strlen(best->path))
          best = p;
      }
    }
    if (best)
      start = best->next; // continue after the directory where current file was found
  }

  // Do NOT search the including file's own directory nor the CWD; only the remaining include paths.
  for (IncludePath *p = start; p; p = p->next) {
    char *full = join_paths(p->path, name);
    char *src = try_read_path(full, resolved_name);
    free(full);
    if (src)
      return src;
  }
  return NULL;
}
