#ifndef CODEGEN_OPT_INTERNAL_H
#define CODEGEN_OPT_INTERNAL_H

#include "mir.h"

void emit_mir_function(const MirFunction *mf);
void optimize_mir_mem2reg(MirFunction *mf);

#endif // CODEGEN_OPT_INTERNAL_H
