#include "../codegen_internal.h"
#include "internal.h"
#include "diagnostics.h"

void emit_mir_function_internal(const MirFunction *mf) {
  if (!mf || !mf->fn)
    error("invalid MIR function in asm emitter");
  emit_mir_function_codegen(mf);
}
