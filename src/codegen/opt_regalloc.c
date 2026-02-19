#include "opt_regalloc.h"
#include "diagnostics.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  int vreg;
  int start;
  int end;
} Interval;

static const char *k_ra_preg64[RA_PREG_COUNT] = {"rbx", "r12", "r13", "r14", "r15"};

const char *ra_preg64(int preg) {
  if (preg < 0 || preg >= RA_PREG_COUNT)
    error("invalid preg id [in ra_preg64]");
  return k_ra_preg64[preg];
}

static int bit_words(int nbits) {
  if (nbits <= 0)
    return 1;
  return (nbits + 63) / 64;
}

static unsigned long long bit_mask(int bit) { return 1ULL << (bit & 63); }

static void bit_set(unsigned long long *bits, int bit) { bits[bit / 64] |= bit_mask(bit); }

static int bit_get(const unsigned long long *bits, int bit) { return (bits[bit / 64] & bit_mask(bit)) != 0; }

static unsigned long long *bit_row(unsigned long long *rows, int row, int words) { return rows + (row * words); }

static const unsigned long long *cbit_row(const unsigned long long *rows, int row, int words) {
  return rows + (row * words);
}

static void bit_or(unsigned long long *dst, const unsigned long long *src, int words) {
  for (int i = 0; i < words; i++)
    dst[i] |= src[i];
}

static void bit_copy(unsigned long long *dst, const unsigned long long *src, int words) {
  memcpy(dst, src, sizeof(unsigned long long) * words);
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

static int alloc_free_preg(const int *preg_in_use) {
  for (int p = 0; p < RA_PREG_COUNT; p++) {
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

static void dump_active(FILE *out, Interval **active, int active_cnt, const int *reg_for_vreg) {
  if (!out)
    return;
  fprintf(out, "    active:");
  if (active_cnt <= 0) {
    fprintf(out, " (empty)\n");
    return;
  }
  for (int i = 0; i < active_cnt; i++) {
    Interval *it = active[i];
    int preg = reg_for_vreg[it->vreg];
    if (preg >= 0)
      fprintf(out, " v%d[%d,%d]=%s", it->vreg, it->start, it->end, ra_preg64(preg));
    else
      fprintf(out, " v%d[%d,%d]=<none>", it->vreg, it->start, it->end);
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
      fprintf(trace_out, "RA trace %.*s (insts=%d, vregs=%d)\n", mf->fn->len, mf->fn->name, mf->inst_len, mf->next_vreg);
    else
      fprintf(trace_out, "RA trace <null-fn> (insts=%d, vregs=%d)\n", mf->inst_len, mf->next_vreg);
  }

  out->locs = calloc(mf->next_vreg, sizeof(RaVRegLoc));
  if (!out->locs)
    error("memory allocation failed [in regalloc_run locs]");
  for (int v = 0; v < mf->next_vreg; v++) {
    out->locs[v].kind = RA_LOC_NONE;
    out->locs[v].reg = -1;
    out->locs[v].stack_slot = -1;
  }

  int ninst = mf->inst_len;
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
    collect_use_def(&mf->insts[i], bit_row(use, i, words), bit_row(def, i, words), mf->next_vreg);
    if (mf->insts[i].op == MIR_OP_LABEL) {
      int l = mf->insts[i].label;
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
    MirInst *in_i = &mf->insts[i];
    if (in_i->op == MIR_OP_RET) {
      continue;
    } else if (in_i->op == MIR_OP_JMP) {
      succ0[i] = label_to_inst ? label_to_inst[in_i->label] : -1;
    } else if (in_i->op == MIR_OP_JZ) {
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
  if (!start || !end)
    error("memory allocation failed [in regalloc_run ranges]");
  for (int v = 0; v < mf->next_vreg; v++) {
    start[v] = -1;
    end[v] = -1;
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

  int interval_cnt = 0;
  for (int v = 0; v < mf->next_vreg; v++) {
    if (start[v] >= 0 && end[v] >= start[v])
      interval_cnt++;
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

  for (int i = 0; i < interval_cnt; i++) {
    Interval *cur = &intervals[i];
    if (dump_trace)
      fprintf(trace_out, "  scan v%d[%d,%d]\n", cur->vreg, cur->start, cur->end);

    int write_idx = 0;
    for (int a = 0; a < active_cnt; a++) {
      Interval *it = active[a];
      if (it->end < cur->start) {
        int preg = reg_for_vreg[it->vreg];
        if (preg >= 0)
          preg_in_use[preg] = 0;
        if (dump_trace && preg >= 0)
          fprintf(trace_out, "    expire v%d[%d,%d] from %s\n", it->vreg, it->start, it->end, ra_preg64(preg));
      } else {
        active[write_idx++] = it;
      }
    }
    active_cnt = write_idx;
    if (active_cnt > 1)
      qsort(active, active_cnt, sizeof(Interval *), cmp_active_end);

    int preg = alloc_free_preg(preg_in_use);
    if (preg >= 0) {
      reg_for_vreg[cur->vreg] = preg;
      preg_in_use[preg] = 1;
      active[active_cnt++] = cur;
      if (active_cnt > 1)
        qsort(active, active_cnt, sizeof(Interval *), cmp_active_end);
      if (dump_trace) {
        fprintf(trace_out, "    assign v%d -> %s\n", cur->vreg, ra_preg64(preg));
        dump_active(trace_out, active, active_cnt, reg_for_vreg);
      }
      continue;
    }

    if (active_cnt <= 0)
      error("regalloc internal error: no active interval with full regs");

    Interval *victim = active[active_cnt - 1];
    int victim_reg = reg_for_vreg[victim->vreg];
    if (victim_reg < 0)
      error("regalloc internal error: victim has no register");

    if (victim->end > cur->end) {
      if (slot_for_vreg[victim->vreg] < 0)
        slot_for_vreg[victim->vreg] = spill_count++;
      reg_for_vreg[victim->vreg] = -1;

      reg_for_vreg[cur->vreg] = victim_reg;
      active[active_cnt - 1] = cur;
      qsort(active, active_cnt, sizeof(Interval *), cmp_active_end);
      if (dump_trace) {
        fprintf(trace_out, "    spill v%d -> slot%d\n", victim->vreg, slot_for_vreg[victim->vreg]);
        fprintf(trace_out, "    assign v%d -> %s\n", cur->vreg, ra_preg64(victim_reg));
        dump_active(trace_out, active, active_cnt, reg_for_vreg);
      }
    } else {
      if (slot_for_vreg[cur->vreg] < 0)
        slot_for_vreg[cur->vreg] = spill_count++;
      if (dump_trace) {
        fprintf(trace_out, "    spill v%d -> slot%d\n", cur->vreg, slot_for_vreg[cur->vreg]);
        dump_active(trace_out, active, active_cnt, reg_for_vreg);
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
      if (out->locs[v].kind == RA_LOC_REG)
        fprintf(trace_out, " v%d=%s", v, ra_preg64(out->locs[v].reg));
      else
        fprintf(trace_out, " v%d=slot%d", v, out->locs[v].stack_slot);
    }
    fprintf(trace_out, "\n");
  }

  free(active);
  free(slot_for_vreg);
  free(reg_for_vreg);
  free(preg_in_use);
  free(intervals);
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
