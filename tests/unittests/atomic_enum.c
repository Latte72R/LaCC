// Verify typedef enum with macro-defined initializers.

#define MO_RELAXED 0
#define MO_CONSUME 1
#define MO_ACQUIRE 2
#define MO_RELEASE 3
#define MO_ACQ_REL 4
#define MO_SEQ_CST 5

typedef enum {
  memory_order_relaxed = MO_RELAXED,
  memory_order_consume = MO_CONSUME,
  memory_order_acquire = MO_ACQUIRE,
  memory_order_release = MO_RELEASE,
  memory_order_acq_rel = MO_ACQ_REL,
  memory_order_seq_cst = MO_SEQ_CST
} memory_order;

int atomic_enum_test1() {
  memory_order ord = memory_order_release;
  return ord + memory_order_relaxed + memory_order_seq_cst;
}
