
#include "lacc.h"

extern FILE *fp;
extern void *NULL;
extern IncludePath *include_paths;

// 指定されたファイルの内容を返す
char *read_file(char *path) {
  // ファイルを開く
  FILE *fp = fopen(path, "r");
  if (!fp)
    return NULL;

  // ファイルの長さを調べる
  if (fseek(fp, 0, SEEK_END) == -1)
    error("%s: fseek", path);
  int size = ftell(fp);
  if (fseek(fp, 0, SEEK_SET) == -1)
    error("%s: fseek", path);

  // ファイル内容を読み込む
  char *buf = malloc(size + 2);
  fread(buf, size, 1, fp);

  // ファイルが必ず"\n\0"で終わっているようにする
  if (size == 0 || buf[size - 1] != '\n')
    buf[size++] = '\n';
  buf[size] = '\0';
  fclose(fp);
  return buf;
}

char *find_file_includes(char *name) {
  char *src = read_file(name);
  if (src)
    return src;

  for (IncludePath *ip = include_paths; ip; ip = ip->next) {
    int plen = strlen(ip->path);
    int nlen = strlen(name);
    char *full = malloc(plen + 1 + nlen + 1);
    memcpy(full, ip->path, plen);
    full[plen] = '/';
    memcpy(full + plen + 1, name, nlen + 1);
    src = read_file(full);
    free(full);
    if (src)
      return src;
  }
  return NULL;
}
