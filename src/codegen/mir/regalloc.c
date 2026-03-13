#include "../codegen_internal.h"
#include "../bitset.h"
#include "diagnostics.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  int vreg;
  int start;
  int end;
} Interval;

static const char *k_ra_preg64[RA_PREG_COUNT] = {"rbx", "r12", "r13", "r14", "r15", "r8", "r9", "r10"};
static const int k_ra_preg_callee_saved[RA_PREG_COUNT] = {1, 1, 1, 1, 1, 0, 0, 0};
static const int k_ra_pref_callee_first[RA_PREG_COUNT] = {0, 1, 2, 3, 4, 5, 6, 7};
static const int k_ra_pref_caller_first[RA_PREG_COUNT] = {5, 6, 7, 0, 1, 2, 3, 4};

const char *ra_preg64(int preg) {
  if (preg < 0 || preg >= RA_PREG_COUNT)
    error("invalid preg id [in ra_preg64]");
  return k_ra_preg64[preg];
}

int ra_preg_is_callee_saved(int preg) {
  if (preg < 0 || preg >= RA_PREG_COUNT)
    error("invalid preg id [in ra_preg_is_callee_saved]");
  return k_ra_preg_callee_saved[preg];
}

static void collect_use_def(const MirInst *in, unsigned long long *use, unsigned long long *def, int vreg_count) {
  if (!in)
    return;

  if (in->dst != MIR_INVALID_VREG) {
    if (in->dst < 0 || in->dst >= vreg_count)
      error("invalid dst vreg in regalloc");
    bit_set(def, in->dst);
  }

  if (in->src1 != MIR_INVALID_VREG) {
    if (in->src1 < 0 || in->src1 >= vreg_count)
      error("invalid src1 vreg in regalloc");
    bit_set(use, in->src1);
  }

  if (in->src2 != MIR_INVALID_VREG) {
    if (in->src2 < 0 || in->src2 >= vreg_count)
      error("invalid src2 vreg in regalloc");
    bit_set(use, in->src2);
  }

  if (in->op == MIR_OP_CALL) {
    for (int i = 0; i < in->argc; i++) {
      if (in->args[i] < 0 || in->args[i] >= vreg_count)
        error("invalid call arg vreg in regalloc");
      bit_set(use, in->args[i]);
    }
  }
}

static int cmp_interval_start(const void *a, const void *b) {
  const Interval *ia = a;
  const Interval *ib = b;
  if (ia->start != ib->start)
    return ia->start - ib->start;
  return ia->end - ib->end;
}

static int cmp_active_end(const void *a, const void *b) {
  const Interval *ia = *(const Interval **)a;
  const Interval *ib = *(const Interval **)b;
  return ia->end - ib->end;
}

static int can_coalesce_with_src1(MirOp op) {
  switch (op) {
  case MIR_OP_MOV:
  case MIR_OP_CAST:
  case MIR_OP_ADD:
  case MIR_OP_SUB:
  case MIR_OP_MUL:
  case MIR_OP_BITAND:
  case MIR_OP_BITOR:
  case MIR_OP_BITXOR:
  case MIR_OP_SHL:
  case MIR_OP_SHR:
  case MIR_OP_BITNOT:
    return 1;
  default:
    return 0;
  }
}

static int alloc_free_preg(const int *preg_in_use, unsigned allowed_mask, const int *pref_order) {
  for (int i = 0; i < RA_PREG_COUNT; i++) {
    int p = pref_order[i];
    if (p < 0 || p >= RA_PREG_COUNT)
      error("invalid preg preference entry [in alloc_free_preg]");
    if (!(allowed_mask & (1u << p)))
      continue;
    if (!preg_in_use[p])
      return p;
  }
  return -1;
}

static int env_is_on(const char *name) {
  const char *v = getenv(name);
  return v && v[0] && v[0] != '0';
}

static int str_eq_len(const char *a, int alen, const char *b) {
  if (!a || !b)
    return 0;
  int blen = strlen(b);
  return alen == blen && !strncmp(a, b, alen);
}

static int should_dump_ra_trace(const MirFunction *mf) {
  if (!env_is_on("LACC_DUMP_RA") && !env_is_on("LACC_DUMP_MIR"))
    return 0;
  const char *filter = getenv("LACC_DUMP_RA_FN");
  if (!filter || !filter[0])
    return 1;
  if (!mf || !mf->fn || !mf->fn->name)
    return 0;
  return str_eq_len(mf->fn->name, mf->fn->len, filter);
}

static void trace_print_preg(FILE *out, int preg) {
  if (!out)
    return;
  if (preg < 0 || preg >= RA_PREG_COUNT) {
    fprintf(out, "<bad-preg:%d>", preg);
    return;
  }
  fputs(ra_preg64(preg), out);
}

static void dump_active(FILE *out, Interval **active, int active_cnt, const int *reg_for_vreg, int vreg_count) {
  if (!out || !active || !reg_for_vreg || vreg_count <= 0)
    return;
  fprintf(out, "    active:");
  if (active_cnt <= 0) {
    fprintf(out, " (empty)\n");
    return;
  }
  for (int i = 0; i < active_cnt; i++) {
    Interval *it = active[i];
    if (!it) {
      fprintf(out, " <null-interval>");
      continue;
    }
    if (it->vreg < 0 || it->vreg >= vreg_count) {
      fprintf(out, " v%d[%d,%d]=<bad-vreg>", it->vreg, it->start, it->end);
      continue;
    }
    int preg = reg_for_vreg[it->vreg];
      if (preg >= 0 && preg < RA_PREG_COUNT) {
      fprintf(out, " v%d[%d,%d]=", it->vreg, it->start, it->end);
      trace_print_preg(out, preg);
      continue;
    }
    if (preg == -1) {
      fprintf(out, " v%d[%d,%d]=<none>", it->vreg, it->start, it->end);
      continue;
    }
    fprintf(out, " v%d[%d,%d]=<bad-preg:%d>", it->vreg, it->start, it->end, preg);
  }
  fprintf(out, "\n");
}

void regalloc_run(const MirFunction *mf, RegAllocResult *out) {
  if (!mf || !out)
    error("invalid input [in regalloc_run]");

  memset(out, 0, sizeof(*out));
  out->vreg_count = mf->next_vreg;
  if (mf->next_vreg <= 0)
    return;
  int dump_trace = should_dump_ra_trace(mf);
  FILE *trace_out = stderr;
  if (dump_trace) {
    if (mf->fn && mf->fn->name)
      fprintf(trace_out, "RA trace %.*s (insts=%d, vregs=%d)\n", mf->fn->len, mf->fn->name, mf->blocks[0].inst_len, mf->next_vreg);
    else
      fprintf(trace_out, "RA trace <null-fn> (insts=%d, vregs=%d)\n", mf->blocks[0].inst_len, mf->next_vreg);
  }

  out->locs = calloc(mf->next_vreg, sizeof(RaVRegLoc));
  if (!out->locs)
    error("memory allocation failed [in regalloc_run locs]");
  for (int v = 0; v < mf->next_vreg; v++) {
    out->locs[v].kind = RA_LOC_NONE;
    out->locs[v].reg = -1;
    out->locs[v].stack_slot = -1;
  }

  int ninst = mf->blocks[0].inst_len;
  int words = bit_words(mf->next_vreg);
  unsigned long long *use = calloc(ninst * words, sizeof(unsigned long long));
  unsigned long long *def = calloc(ninst * words, sizeof(unsigned long long));
  unsigned long long *in = calloc(ninst * words, sizeof(unsigned long long));
  unsigned long long *out_bits = calloc(ninst * words, sizeof(unsigned long long));
  if (!use || !def || !in || !out_bits)
    error("memory allocation failed [in regalloc_run bitsets]");

  int *label_to_inst = NULL;
  if (mf->next_label > 0) {
    label_to_inst = malloc(sizeof(int) * mf->next_label);
    if (!label_to_inst)
      error("memory allocation failed [in regalloc_run label map]");
    for (int i = 0; i < mf->next_label; i++)
      label_to_inst[i] = -1;
  }

  for (int i = 0; i < ninst; i++) {
    collect_use_def(&mf->blocks[0].insts[i], bit_row(use, i, words), bit_row(def, i, words), mf->next_vreg);
    if (mf->blocks[0].insts[i].op == MIR_OP_LABEL) {
      int l = mf->blocks[0].insts[i].label;
      if (l < 0 || l >= mf->next_label)
        error("invalid label id in MIR_OP_LABEL [in regalloc_run]");
      label_to_inst[l] = i;
    }
  }

  int *succ0 = malloc(sizeof(int) * ninst);
  int *succ1 = malloc(sizeof(int) * ninst);
  if (!succ0 || !succ1)
    error("memory allocation failed [in regalloc_run succ]");
  for (int i = 0; i < ninst; i++) {
    succ0[i] = -1;
    succ1[i] = -1;
    MirInst *in_i = &mf->blocks[0].insts[i];
    if (in_i->op == MIR_OP_RET) {
      continue;
    } else if (in_i->op == MIR_OP_JMP) {
      succ0[i] = label_to_inst ? label_to_inst[in_i->label] : -1;
    } else if (in_i->op == MIR_OP_JZ || in_i->op == MIR_OP_JCC) {
      succ0[i] = (i + 1 < ninst) ? (i + 1) : -1;
      succ1[i] = label_to_inst ? label_to_inst[in_i->label] : -1;
    } else {
      succ0[i] = (i + 1 < ninst) ? (i + 1) : -1;
    }
  }
  // out[i]: この命令の直後に live な集合
  unsigned long long *tmp_out = malloc(sizeof(unsigned long long) * words);
  // in[i]: この命令の直前に live な集合
  unsigned long long *tmp_in = malloc(sizeof(unsigned long long) * words);
  if (!tmp_out || !tmp_in)
    error("memory allocation failed [in regalloc_run tmp]");

  int changed = 1;
  while (changed) {
    changed = 0;
    for (int i = ninst - 1; i >= 0; i--) {
      // out[i] = in[succ0[i]] ∪ in[succ1[i]]
      memset(tmp_out, 0, sizeof(unsigned long long) * words);
      if (succ0[i] >= 0)
        bit_or(tmp_out, cbit_row(in, succ0[i], words), words);
      if (succ1[i] >= 0)
        bit_or(tmp_out, cbit_row(in, succ1[i], words), words);

      // in[i] = use[i] ∪ (out[i] - def[i])
      bit_copy(tmp_in, cbit_row(use, i, words), words);
      for (int w = 0; w < words; w++) {
        unsigned long long live_out_minus_def = tmp_out[w] & ~cbit_row(def, i, words)[w];
        tmp_in[w] |= live_out_minus_def;
      }

      if (memcmp(cbit_row(out_bits, i, words), tmp_out, sizeof(unsigned long long) * words) != 0) {
        bit_copy(bit_row(out_bits, i, words), tmp_out, words);
        changed = 1;
      }
      if (memcmp(cbit_row(in, i, words), tmp_in, sizeof(unsigned long long) * words) != 0) {
        bit_copy(bit_row(in, i, words), tmp_in, words);
        changed = 1;
      }
    }
  }

  int *start = malloc(sizeof(int) * mf->next_vreg);
  int *end = malloc(sizeof(int) * mf->next_vreg);
  int *copy_src_at_start = malloc(sizeof(int) * mf->next_vreg);
  int *crosses_call = calloc(mf->next_vreg, sizeof(int));
  int *use_site_count = calloc(mf->next_vreg, sizeof(int));
  if (!start || !end || !copy_src_at_start || !crosses_call || !use_site_count)
    error("memory allocation failed [in regalloc_run ranges]");
  for (int v = 0; v < mf->next_vreg; v++) {
    start[v] = -1;
    end[v] = -1;
    copy_src_at_start[v] = MIR_INVALID_VREG;
  }

  for (int i = 0; i < ninst; i++) {
    // touched = use[i][v] || def[i][v] || in[i][v] || out[i][v]
    const unsigned long long *use_i = cbit_row(use, i, words);
    const unsigned long long *def_i = cbit_row(def, i, words);
    const unsigned long long *in_i = cbit_row(in, i, words);
    const unsigned long long *out_i = cbit_row(out_bits, i, words);
    for (int v = 0; v < mf->next_vreg; v++) {
      int touched = bit_get(use_i, v) || bit_get(def_i, v) || bit_get(in_i, v) || bit_get(out_i, v);
      if (!touched)
        continue;
      // start[v] = min(start[v], i)
      if (start[v] < 0 || i < start[v])
        start[v] = i;
      // end[v] = max(end[v], i)
      if (i > end[v])
        end[v] = i;
    }
  }

  for (int i = 0; i < ninst; i++) {
    const unsigned long long *use_i = cbit_row(use, i, words);
    for (int v = 0; v < mf->next_vreg; v++) {
      if (bit_get(use_i, v))
        use_site_count[v]++;
    }
    if (mf->blocks[0].insts[i].op != MIR_OP_CALL)
      continue;
    const unsigned long long *in_i = cbit_row(in, i, words);
    const unsigned long long *out_i = cbit_row(out_bits, i, words);
    for (int v = 0; v < mf->next_vreg; v++) {
      if (bit_get(in_i, v) && bit_get(out_i, v))
        crosses_call[v] = 1;
    }
  }

  int interval_cnt = 0;
  for (int v = 0; v < mf->next_vreg; v++) {
    if (start[v] >= 0 && end[v] >= start[v])
      interval_cnt++;
  }

  // coalescing hint:
  // if interval v starts with a src1-dominant op and src1 dies at this
  // instruction, prefer assigning the same physical register to v.
  for (int v = 0; v < mf->next_vreg; v++) {
    int s = start[v];
    if (s < 0 || s >= ninst)
      continue;
    MirInst *in_s = &mf->blocks[0].insts[s];
    if (!can_coalesce_with_src1(in_s->op) || in_s->dst != v || in_s->src1 == MIR_INVALID_VREG)
      continue;
    if (in_s->src1 < 0 || in_s->src1 >= mf->next_vreg)
      error("invalid src1 vreg for copy coalescing hint");
    copy_src_at_start[v] = in_s->src1;
  }

  Interval *intervals = malloc(sizeof(Interval) * (interval_cnt > 0 ? interval_cnt : 1));
  if (!intervals)
    error("memory allocation failed [in regalloc_run intervals]");
  int k = 0;
  for (int v = 0; v < mf->next_vreg; v++) {
    if (start[v] >= 0 && end[v] >= start[v]) {
      intervals[k].vreg = v;
      intervals[k].start = start[v];
      intervals[k].end = end[v];
      k++;
    }
  }
  qsort(intervals, interval_cnt, sizeof(Interval), cmp_interval_start);
  if (dump_trace) {
    fprintf(trace_out, "  intervals:");
    for (int i = 0; i < interval_cnt; i++)
      fprintf(trace_out, " v%d[%d,%d]", intervals[i].vreg, intervals[i].start, intervals[i].end);
    fprintf(trace_out, "\n");
  }

  int *preg_in_use = calloc(RA_PREG_COUNT, sizeof(int));
  int *reg_for_vreg = malloc(sizeof(int) * mf->next_vreg);
  int *slot_for_vreg = malloc(sizeof(int) * mf->next_vreg);
  Interval **active = malloc(sizeof(Interval *) * (interval_cnt > 0 ? interval_cnt : 1));
  if (!preg_in_use || !reg_for_vreg || !slot_for_vreg || !active)
    error("memory allocation failed [in regalloc_run state]");
  for (int v = 0; v < mf->next_vreg; v++) {
    reg_for_vreg[v] = -1;
    slot_for_vreg[v] = -1;
  }

  int active_cnt = 0;
  int spill_count = 0;
  int prefer_tiny_callspill = ninst <= 16;
  unsigned callee_saved_mask = 0;
  for (int p = 0; p < RA_PREG_COUNT; p++) {
    if (ra_preg_is_callee_saved(p))
      callee_saved_mask |= 1u << p;
  }
  unsigned all_reg_mask = callee_saved_mask;
  for (int p = 0; p < RA_PREG_COUNT; p++) {
    if (!ra_preg_is_callee_saved(p))
      all_reg_mask |= 1u << p;
  }

  for (int i = 0; i < interval_cnt; i++) {
    Interval *cur = &intervals[i];
    int copy_src = copy_src_at_start[cur->vreg];
    int preferred_preg = -1;
    unsigned allowed_mask = crosses_call[cur->vreg] ? callee_saved_mask : all_reg_mask;
    const int *alloc_pref = crosses_call[cur->vreg] ? k_ra_pref_callee_first : k_ra_pref_caller_first;
    if (dump_trace)
      fprintf(trace_out, "  scan v%d[%d,%d] allow=%s\n", cur->vreg, cur->start, cur->end,
              crosses_call[cur->vreg] ? "callee-saved" : "all");

    int write_idx = 0;
    for (int a = 0; a < active_cnt; a++) {
      Interval *it = active[a];
      int expire_for_copy = (copy_src != MIR_INVALID_VREG && it->vreg == copy_src && it->end == cur->start);
      if (it->end < cur->start || expire_for_copy) {
        int preg = reg_for_vreg[it->vreg];
        if (preg >= 0) {
          preg_in_use[preg] = 0;
          if (expire_for_copy)
            preferred_preg = preg;
        }
        if (dump_trace && preg >= 0) {
          if (expire_for_copy)
            fprintf(trace_out, "    expire(copy) v%d[%d,%d] from ", it->vreg, it->start, it->end);
          else
            fprintf(trace_out, "    expire v%d[%d,%d] from ", it->vreg, it->start, it->end);
          trace_print_preg(trace_out, preg);
          fputc('\n', trace_out);
        }
      } else {
        active[write_idx++] = it;
      }
    }
    active_cnt = write_idx;
    if (active_cnt > 1)
      qsort(active, active_cnt, sizeof(Interval *), cmp_active_end);

    if (preferred_preg < 0 && copy_src != MIR_INVALID_VREG) {
      int cand = reg_for_vreg[copy_src];
      if (cand >= 0 && (allowed_mask & (1u << cand)) && !preg_in_use[cand])
        preferred_preg = cand;
    }

    if (prefer_tiny_callspill && crosses_call[cur->vreg] && use_site_count[cur->vreg] <= 3 &&
        (cur->end - cur->start) <= 16)
      allowed_mask = 0;

    int preg = -1;
    if (preferred_preg >= 0 && preferred_preg < RA_PREG_COUNT && (allowed_mask & (1u << preferred_preg)) &&
        !preg_in_use[preferred_preg])
      preg = preferred_preg;
    else
      preg = alloc_free_preg(preg_in_use, allowed_mask, alloc_pref);
    if (preg >= 0) {
      reg_for_vreg[cur->vreg] = preg;
      preg_in_use[preg] = 1;
      active[active_cnt++] = cur;
      if (active_cnt > 1)
        qsort(active, active_cnt, sizeof(Interval *), cmp_active_end);
      if (dump_trace) {
        if (copy_src != MIR_INVALID_VREG && preferred_preg == preg)
          fprintf(trace_out, "    assign(coalesce v%d<-v%d) v%d -> ", cur->vreg, copy_src, cur->vreg);
        else
          fprintf(trace_out, "    assign v%d -> ", cur->vreg);
        trace_print_preg(trace_out, preg);
        fputc('\n', trace_out);
        dump_active(trace_out, active, active_cnt, reg_for_vreg, mf->next_vreg);
      }
      continue;
    }

    Interval *victim = NULL;
    int victim_idx = -1;
    int victim_reg = -1;
    for (int a = 0; a < active_cnt; a++) {
      Interval *it = active[a];
      int preg_it = reg_for_vreg[it->vreg];
      if (preg_it < 0 || !(allowed_mask & (1u << preg_it)))
        continue;
      if (!victim || it->end > victim->end) {
        victim = it;
        victim_idx = a;
        victim_reg = preg_it;
      }
    }

    if (victim && victim->end > cur->end) {
      if (slot_for_vreg[victim->vreg] < 0)
        slot_for_vreg[victim->vreg] = spill_count++;
      reg_for_vreg[victim->vreg] = -1;

      reg_for_vreg[cur->vreg] = victim_reg;
      active[victim_idx] = cur;
      qsort(active, active_cnt, sizeof(Interval *), cmp_active_end);
      if (dump_trace) {
        fprintf(trace_out, "    spill v%d -> slot%d\n", victim->vreg, slot_for_vreg[victim->vreg]);
        fprintf(trace_out, "    assign v%d -> ", cur->vreg);
        trace_print_preg(trace_out, victim_reg);
        fputc('\n', trace_out);
        dump_active(trace_out, active, active_cnt, reg_for_vreg, mf->next_vreg);
      }
    } else {
      if (slot_for_vreg[cur->vreg] < 0)
        slot_for_vreg[cur->vreg] = spill_count++;
      if (dump_trace) {
        fprintf(trace_out, "    spill v%d -> slot%d\n", cur->vreg, slot_for_vreg[cur->vreg]);
        dump_active(trace_out, active, active_cnt, reg_for_vreg, mf->next_vreg);
      }
    }
  }

  out->spill_count = spill_count;
  out->used_reg_mask = 0;
  for (int v = 0; v < mf->next_vreg; v++) {
    if (reg_for_vreg[v] >= 0) {
      out->locs[v].kind = RA_LOC_REG;
      out->locs[v].reg = reg_for_vreg[v];
      out->locs[v].stack_slot = -1;
      out->used_reg_mask |= 1u << reg_for_vreg[v];
    } else if (slot_for_vreg[v] >= 0) {
      out->locs[v].kind = RA_LOC_STACK;
      out->locs[v].reg = -1;
      out->locs[v].stack_slot = slot_for_vreg[v];
    } else {
      out->locs[v].kind = RA_LOC_STACK;
      out->locs[v].reg = -1;
      out->locs[v].stack_slot = out->spill_count++;
    }
  }
  if (dump_trace) {
    fprintf(trace_out, "  result:");
    for (int v = 0; v < mf->next_vreg; v++) {
      if (out->locs[v].kind == RA_LOC_REG) {
        fprintf(trace_out, " v%d=", v);
        trace_print_preg(trace_out, out->locs[v].reg);
      } else {
        fprintf(trace_out, " v%d=slot%d", v, out->locs[v].stack_slot);
      }
    }
    fprintf(trace_out, "\n");
  }

  free(active);
  free(slot_for_vreg);
  free(reg_for_vreg);
  free(preg_in_use);
  free(intervals);
  free(use_site_count);
  free(crosses_call);
  free(copy_src_at_start);
  free(end);
  free(start);
  free(tmp_in);
  free(tmp_out);
  free(succ1);
  free(succ0);
  free(label_to_inst);
  free(out_bits);
  free(in);
  free(def);
  free(use);
}

void regalloc_free(RegAllocResult *out) {
  if (!out)
    return;
  free(out->locs);
  out->locs = NULL;
  out->vreg_count = 0;
  out->spill_count = 0;
  out->used_reg_mask = 0;
}
