#ifndef CODEGEN_MEM2REG_H
#define CODEGEN_MEM2REG_H

#include "mir.h"

void optimize_mir_mem2reg(MirFunction *mf);
void optimize_mir_inline_cleanup(MirFunction *mf);

#endif // CODEGEN_MEM2REG_H
