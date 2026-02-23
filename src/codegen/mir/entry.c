#include "../codegen_internal.h"
#include "internal.h"
#include "diagnostics.h"

void emit_mir_function_internal(const MirFunction *mf) {
  if (mf && mf->fn) {
    emit_mir_function_codegen(mf);
    return;
  }
  error("invalid MIR function in asm emitter");
}
