SRC_DIR:=./src
INCLUDE_DIR:=./include
TEST_DIR:=./tests
EXAMPLE_DIR:=./examples
BUILD_DIR:=./build
CC_FLAGS_1:=-std=c99 -I $(INCLUDE_DIR) -w
CC_FLAGS_2:=-std=c99
CC_FLAGS_3:=-std=c99 -I $(INCLUDE_DIR) \
	-O0 -w -g -fsanitize=address,undefined -fno-omit-frame-pointer
CC_FLAGS:=$(CC_FLAGS_1)
LACC_FLAGS:=-I $(INCLUDE_DIR)
EXTENSION:=$(SRC_DIR)/extension.c
HEADERS:=$(wildcard $(INCLUDE_DIR)/*.h)
SRCS:=$(shell find $(SRC_DIR) -type f -name '*.c' | sort)
SRCS:=$(filter-out $(EXTENSION),$(SRCS))
OBJ_S:=$(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.s,$(SRCS))
CC?=cc
NO_PRINT_DIR:=--no-print-directory
BOOSTSTRAP:=$(BUILD_DIR)/bootstrap
SELFHOST:=$(BUILD_DIR)/lacc
UNIT_TEST:=$(TEST_DIR)/unittest.c
WARN_TEST:=$(TEST_DIR)/warntest.c
INCLUDE_TEST:=$(TEST_DIR)/includes.c
TMP_C:=$(BUILD_DIR)/tmp.c
TMP_S:=$(BUILD_DIR)/tmp.s
TMP_S_O0:=$(BUILD_DIR)/tmp_O0.s
TMP_S_O1:=$(BUILD_DIR)/tmp_O1.s
TMP_OUT:=$(BUILD_DIR)/tmp
STAGE3_DIR := $(BUILD_DIR)/stage3

.DEFAULT_GOAL := help
help:
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}'

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
	@echo "Build directory created at '$@'."

bootstrap: $(BOOSTSTRAP) ## Build the bootstrap compiler

selfhost: $(SELFHOST) ## Build the self-hosted compiler

define runfile
	@mkdir -p ${BUILD_DIR}
	@$(1) $(LACC_FLAGS) $(2) -S -o $(TMP_S)
	@cc -o ${BUILD_DIR}/tmp $(TMP_S)
	@${BUILD_DIR}/tmp
endef

run: .run-selfhost ## Run a file with the self-hosted compiler

.run-cc: | $(BUILD_DIR)
	@$(call runfile, $(CC), $(FILE))

.run-bootstrap: $(BOOSTSTRAP) | $(BUILD_DIR)
	@$(call runfile, $(BOOSTSTRAP), $(FILE))

.run-selfhost: $(SELFHOST) | $(BUILD_DIR)
	@$(call runfile, $(SELFHOST), $(FILE))

$(BOOSTSTRAP): $(SRCS) $(EXTENSION) $(HEADERS) | $(BUILD_DIR)
	@$(CC) $(CC_FLAGS) -o $(BOOSTSTRAP) $(SRCS) $(EXTENSION)
	@echo "Bootstrap compiler created at '$@'."

$(BUILD_DIR)/%.s: $(SRC_DIR)/%.c $(HEADERS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@$(BOOSTSTRAP) $(LACC_FLAGS) $< -o $@

$(SELFHOST): $(BOOSTSTRAP) $(OBJ_S) | $(BUILD_DIR)
	@$(CC) -o $@ $(OBJ_S) $(EXTENSION) $(CC_FLAGS)
	@echo "Self-hosted compiler created at '$@'."

define unittest
	@echo "[unittest -O0]"
	@$(call runfile, $(1), -O0 $(UNIT_TEST))
	@echo "[unittest -O1]"
	@$(call runfile, $(1), -O1 $(UNIT_TEST))
endef

define warntest
	@$(call runfile, $(1), $(WARN_TEST))
endef

define errortest
	@$(TEST_DIR)/errortest.sh $(BUILD_DIR) $(1) $(TMP_C) $(TMP_S) $(TMP_OUT)
endef

define includetest
	@$(call runfile, $(1), $(INCLUDE_TEST))
endef

unittest: .unittest-selfhost ## Run unit tests with the self-hosted compiler

warntest: .warntest-selfhost ## Run warning tests with the self-hosted compiler

errortest: .errortest-selfhost ## Run error tests with the self-hosted compiler

includetest: .includetest-selfhost ## Run include tests with the self-hosted compiler

.unittest-cc: | $(BUILD_DIR)
	@$(CC) $(CC_FLAGS) -o $(TMP_OUT) $(UNIT_TEST)
	@$(TMP_OUT)

.unittest-bootstrap: $(BOOSTSTRAP) | $(BUILD_DIR)
	@$(call unittest, $(BOOSTSTRAP))
	
.unittest-selfhost: $(SELFHOST) | $(BUILD_DIR)
	@$(call unittest, $(SELFHOST))

.warntest-cc: | $(BUILD_DIR)
	@$(CC) $(CC_FLAGS_2) -o $(TMP_OUT) $(WARN_TEST)
	@$(TMP_OUT)

.warntest-bootstrap: $(BOOSTSTRAP) | $(BUILD_DIR)
	@$(call warntest, $(BOOSTSTRAP))

.warntest-selfhost: $(SELFHOST) | $(BUILD_DIR)
	@$(call warntest, $(SELFHOST))

.errortest-cc: | $(BUILD_DIR)
	@$(call errortest, $(CC))

.errortest-bootstrap: $(BOOSTSTRAP) | $(BUILD_DIR)
	@$(call errortest, $(BOOSTSTRAP))

.errortest-selfhost: $(SELFHOST) | $(BUILD_DIR)
	@$(call errortest, $(SELFHOST))

.includetest-cc: | $(BUILD_DIR)
	@$(CC) $(CC_FLAGS_2) -o $(TMP_OUT) $(INCLUDE_TEST)
	@$(TMP_OUT)

.includetest-bootstrap: $(BOOSTSTRAP) | $(BUILD_DIR)
	@$(call includetest, $(BOOSTSTRAP))

.includetest-selfhost: $(SELFHOST) | $(BUILD_DIR)
	@$(call includetest, $(SELFHOST))

# selfhost で src/*.c → stage3/*.s を生成
$(STAGE3_DIR)/%.s: $(SRC_DIR)/%.c $(SELFHOST) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@$(SELFHOST) $(LACC_FLAGS) $< -o $@

sanitize: CC_FLAGS=$(CC_FLAGS_3) ## Compile sources with sanitizer checks
sanitize: clean bootstrap selfhost clean
	@echo "Sanitizer checks passed. (ASan)"

.NOTPARALLEL: sanitize

asmcmp: $(SELFHOST) | $(BUILD_DIR) ## Compare asm line counts: lacc/clang x -O0/-O1 (default: all src/*.c, optional: FILE=path/to/file.c)
	@set -eu; \
	command -v clang >/dev/null 2>&1 || { echo "clang not found"; exit 1; }; \
	files="$(FILE)"; \
	if [ -z "$$files" ]; then \
	  files="$(SRCS)"; \
	fi; \
	outdir="$(BUILD_DIR)/asmlinecmp"; \
	mkdir -p "$$outdir"; \
	t_l0=0; t_l1=0; t_c0=0; t_c1=0; \
	printf '%-40s %8s %8s %8s %8s\n' "file" "lacc-O0" "lacc-O1" "clang-O0" "clang-O1"; \
	printf '%-40s %8s %8s %8s %8s\n' "----" "-------" "-------" "--------" "--------"; \
	for f in $$files; do \
	  rel=$${f#./}; \
	  base=$${rel%.c}; \
	  l0="$$outdir/$${base}.lacc.O0.s"; \
	  l1="$$outdir/$${base}.lacc.O1.s"; \
	  c0="$$outdir/$${base}.clang.O0.s"; \
	  c1="$$outdir/$${base}.clang.O1.s"; \
	  mkdir -p "$$(dirname "$$l0")"; \
	  $(SELFHOST) $(LACC_FLAGS) -O0 "$$f" -S -o "$$l0"; \
	  $(SELFHOST) $(LACC_FLAGS) -O1 "$$f" -S -o "$$l1"; \
	  clang -std=c99 -I $(INCLUDE_DIR) -w -O0 -S "$$f" -o "$$c0"; \
	  clang -std=c99 -I $(INCLUDE_DIR) -w -O1 -S "$$f" -o "$$c1"; \
	  n_l0=$$(wc -l < "$$l0" | tr -d ' '); \
	  n_l1=$$(wc -l < "$$l1" | tr -d ' '); \
	  n_c0=$$(wc -l < "$$c0" | tr -d ' '); \
	  n_c1=$$(wc -l < "$$c1" | tr -d ' '); \
	  t_l0=$$((t_l0 + n_l0)); \
	  t_l1=$$((t_l1 + n_l1)); \
	  t_c0=$$((t_c0 + n_c0)); \
	  t_c1=$$((t_c1 + n_c1)); \
	  printf '%-40s %8s %8s %8s %8s\n' "$$rel" "$$n_l0" "$$n_l1" "$$n_c0" "$$n_c1"; \
	done; \
	printf '%-40s %8s %8s %8s %8s\n' "TOTAL" "$$t_l0" "$$t_l1" "$$t_c0" "$$t_c1"

clean: ## Clean up generated files
	@rm -rf $(BUILD_DIR)
	@echo "Cleaned up generated files."

.PHONY: help run unittest warntest errortest includetest sanitize asmcmp clean \
        bootstrap selfhost .run-cc .run-bootstrap .run-selfhost \
        .unittest-cc .unittest-bootstrap .unittest-selfhost \
		.warntest-cc .warntest-bootstrap .warntest-selfhost \
		.errortest-cc .errortest-bootstrap .errortest-selfhost \
		.includetest-cc .includetest-bootstrap .includetest-selfhost
