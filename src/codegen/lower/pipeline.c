#include "../codegen_internal.h"
#include "diagnostics.h"
#include "platform.h"
#include "runtime.h"

#include <stdlib.h>
#include <string.h>

static int should_dump_mir(void) {
  const char *env = getenv("LACC_DUMP_MIR");
  return env && env[0] && env[0] != '0';
}

static void assign_local_offsets(MirFunction *mf) {
  if (!mf || mf->local_count <= 0)
    return;
  unsigned char *used = calloc(mf->local_count, sizeof(unsigned char));
  int *offsets = calloc(mf->local_count, sizeof(int));
  if (!used || !offsets)
    error("memory allocation failed [in assign_local_offsets]");

  for (int b = 0; b < mf->block_count; b++) {
    MirBasicBlock *bb = &mf->blocks[b];
    for (int i = 0; i < bb->inst_len; i++) {
      MirInst *in = &bb->insts[i];
      if (in->op != MIR_OP_LOAD_LOCAL && in->op != MIR_OP_STORE_LOCAL && in->op != MIR_OP_ADDR_LOCAL)
        continue;
      if (in->offset <= 0 || in->offset > mf->local_count)
        error("invalid local slot [in assign_local_offsets]");
      used[in->offset - 1] = 1;
    }
  }

  int offset = 0;
  for (int i = 0; i < mf->local_count; i++) {
    if (!used[i])
      continue;
    offset += mf->local_sizes[i];
    offsets[i] = offset;
  }
  for (int b = 0; b < mf->block_count; b++) {
    MirBasicBlock *bb = &mf->blocks[b];
    for (int i = 0; i < bb->inst_len; i++) {
      MirInst *in = &bb->insts[i];
      if (in->op == MIR_OP_LOAD_LOCAL || in->op == MIR_OP_STORE_LOCAL || in->op == MIR_OP_ADDR_LOCAL)
        in->offset = offsets[in->offset - 1];
    }
  }
  for (int i = 0; i < mf->param_count; i++) {
    int slot = mf->param_slots[i];
    mf->param_slots[i] = slot > 0 && slot <= mf->local_count ? offsets[slot - 1] : 0;
  }
  free(offsets);
  free(used);
}

static int find_function(MirFunction *mfs, int count, Function *fn) {
  for (int i = 0; i < count; i++) {
    if (mfs[i].fn == fn)
      return i;
    if (mfs[i].fn->len == fn->len && !strncmp(mfs[i].fn->name, fn->name, fn->len))
      return i;
  }
  return -1;
}

static void mark_reachable_functions(MirFunction *mfs, int count, unsigned char *reachable) {
  int *queue = malloc(sizeof(int) * count);
  if (!queue)
    error("memory allocation failed [in function reachability]");
  int head = 0;
  int tail = 0;
  for (int i = 0; i < count; i++) {
    if (!mfs[i].fn->is_static && !mfs[i].fn->is_inline) {
      reachable[i] = 1;
      queue[tail++] = i;
    }
  }
  while (head < tail) {
    int from = queue[head++];
    for (int b = 0; b < mfs[from].block_count; b++) {
      MirBasicBlock *bb = &mfs[from].blocks[b];
      for (int i = 0; i < bb->inst_len; i++) {
        MirInst *in = &bb->insts[i];
        if ((in->op != MIR_OP_CALL && in->op != MIR_OP_ADDR_FUNC) || !in->call_fn)
          continue;
        int to = find_function(mfs, count, in->call_fn);
        if (to >= 0 && !reachable[to]) {
          reachable[to] = 1;
          queue[tail++] = to;
        }
      }
    }
  }
  free(queue);
}

static void emit_text() {
  int count = 0;
  for (int i = 0; code[i]->kind != ND_NONE; i++)
    count += code[i]->kind == ND_FUNCDEF ? 1 : 0;
  if (count == 0)
    return;
#if LACC_PLATFORM_APPLE
  write_file("  .section __TEXT,__text,regular,pure_instructions\n");
#else
  write_file("  .text\n");
#endif
  MirFunction *mfs = calloc(count > 0 ? count : 1, sizeof(MirFunction));
  unsigned char *reachable = calloc(count > 0 ? count : 1, sizeof(unsigned char));
  if (!mfs || !reachable)
    error("memory allocation failed [in text codegen]");
  int index = 0;
  for (int i = 0; code[i]->kind != ND_NONE; i++) {
    if (code[i]->kind != ND_FUNCDEF)
      continue;
    MirFunction *mf = &mfs[index++];
    mf->current_block = -1;
    lower_function(code[i], mf);
    mir_finalize_cfg(mf);
    if (optimize_level > 0) {
      optimize_mir_ssa(mf);
      destruct_mir_ssa(mf);
    }
  }
  mark_reachable_functions(mfs, count, reachable);
  for (int i = 0; i < count; i++) {
    MirFunction *mf = &mfs[i];
    if (!reachable[i]) {
      mir_free(mf);
      continue;
    }
    if (should_dump_mir())
      mir_dump(stderr, mf);
    assign_local_offsets(mf);
    mir_linearize(mf);
    emit_mir_function_internal(mf);
    mir_free(mf);
  }
  free(reachable);
  free(mfs);
}

void generate_assembly_pipeline() {
  write_file(".intel_syntax noprefix\n");
#if !LACC_PLATFORM_APPLE
  write_file("  .section .note.GNU-stack,\"\",@progbits\n");
#endif
  gen_rodata_section();
  gen_data_section();
  emit_text();
}
