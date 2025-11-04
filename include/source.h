#ifndef SOURCE_H
#define SOURCE_H

typedef struct IncludePath IncludePath;
struct IncludePath {
  char *path;
  IncludePath *next;
};

typedef struct Location Location;
struct Location {
  char *path;
  char *loc;
  char *input;
  const char *line_start;
  const char *line_end;
  int line;
  int column;
};

typedef struct FileName FileName;
struct FileName {
  FileName *next;
  char *name;
};

typedef struct CharPtrList CharPtrList;
struct CharPtrList {
  char *str;
  CharPtrList *next;
};

char *read_file(char *path);
char *read_include_file(char *name, const char *including_file, int is_system, char **resolved_name);
char *read_include_next_file(char *name, const char *including_file, char **resolved_name);

#endif // SOURCE_H
