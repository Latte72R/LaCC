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
SRCS:=$(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/**/*.c)
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
	@$(call runfile, $(1), $(UNIT_TEST))
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

# コンパイラ本体のみ比較（SRCS 全部）
asmcmp: $(SELFHOST) $(patsubst $(SRC_DIR)/%.c,$(STAGE3_DIR)/%.s,$(SRCS)) ## Compare stage2 vs stage3 assembly for compiler sources
	@set -eu; \
	fail=0; \
	green=$$(printf '\033[32m'); \
	red=$$(printf '\033[31m'); \
	reset=$$(printf '\033[0m'); \
	for f in $(SRCS); do \
	  rel=$${f#$(SRC_DIR)/}; \
	  s2="$(BUILD_DIR)/$${rel%.c}.s"; \
	  s3="$(STAGE3_DIR)/$${rel%.c}.s"; \
	  if diff -u "$$s2" "$$s3" >/dev/null; then \
	    printf '%sOK%s: %s\n' "$$green" "$$reset" "$$rel"; \
	  else \
	    printf '%sNG%s: %s\n' "$$red" "$$reset" "$$rel"; \
	    fail=1; \
	  fi; \
	done; \
	if [ "$$fail" -eq 0 ]; then \
	  printf '%sAll files match!%s\n' "$$green" "$$reset"; \
	else \
	  exit 1; \
	fi

clean: ## Clean up generated files
	@rm -rf $(BUILD_DIR)
	@echo "Cleaned up generated files."

.PHONY: help run unittest warntest errortest includetest sanitize asmcmp clean \
        bootstrap selfhost .run-cc .run-bootstrap .run-selfhost \
        .unittest-cc .unittest-bootstrap .unittest-selfhost \
		.warntest-cc .warntest-bootstrap .warntest-selfhost \
		.errortest-cc .errortest-bootstrap .errortest-selfhost \
		.includetest-cc .includetest-bootstrap .includetest-selfhost
