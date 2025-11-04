#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include "source.h"

void write_file(char *fmt, ...);
void error(char *fmt, ...);
void error_at(Location *loc, char *fmt, ...);
void warning(char *fmt, ...);
void warning_at(Location *loc, char *fmt, ...);

#endif // DIAGNOSTICS_H
