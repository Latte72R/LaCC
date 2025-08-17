
#include "lacc.h"

extern FILE *fp;
extern void *NULL;
extern IncludePath *include_paths;

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

char *read_include_file(char *name) {
  // まずは指定された名前のファイルを直接試す
  char *src = read_file(name);
  if (src)
    return src;

  // 見つからなければ登録されたインクルードパスを順番に探索
  for (IncludePath *path = include_paths; path; path = path->next) {
    int len = strlen(path->path) + strlen(name) + 2;
    // ディレクトリとファイル名を結合して完全パスを作成
    char *full = malloc(len);
    snprintf(full, len, "%s/%s", path->path, name);
    src = read_file(full); // 生成したパスでファイルを読み込む
    free(full);
    if (src)
      return src; // 最初に見つかったものを返す
  }
  return NULL; // どのパスにも存在しない
}
