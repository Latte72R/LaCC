CC_FLAGS_2:=-std=c99 -Wno-incompatible-library-redeclaration -Wno-builtin-declaration-mismatch -Wno-unknown-warning-option
CC_FLAGS:=-std=c99 -I include -w
LACC_FLAGS:=-I include
SRC_DIR:=./src
TEST_DIR:=./tests
EXAMPLE_DIR:=./examples
BUILD_DIR:=./build
EXTENSION:=$(SRC_DIR)/extension.c
SRCS:=$(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/**/*.c)
SRCS:=$(filter-out $(EXTENSION),$(SRCS))
OBJ_S:=$(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.s,$(SRCS))
CC:=cc
BOOSTSTRAP:=$(BUILD_DIR)/bootstrap
SELFHOST:=$(BUILD_DIR)/lacc
UNIT_TEST:=$(TEST_DIR)/unittest.c
WARN_TEST:=$(TEST_DIR)/warntest.c
TMP_C:=$(BUILD_DIR)/tmp.c
TMP_S:=$(BUILD_DIR)/tmp.s
TMP_OUT:=$(BUILD_DIR)/tmp

.DEFAULT_GOAL := help
help:
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}'

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
	@echo "Build directory created at '$@'."

bootstrap: $(BOOSTSTRAP)

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

$(BOOSTSTRAP): $(SRCS) $(EXTENSION) | $(BUILD_DIR)
	@$(CC) $(CC_FLAGS) -o $(BOOSTSTRAP) $(SRCS) $(EXTENSION)
	@echo "Bootstrap compiler created at '$@'."

$(BUILD_DIR)/%.s: $(SRC_DIR)/%.c | $(BUILD_DIR)
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

unittest: .unittest-selfhost ## Run unit tests with the self-hosted compiler

warntest: .warntest-selfhost ## Run warning tests with the self-hosted compiler

errortest: .errortest-selfhost ## Run error tests with the self-hosted compiler

.unittest-cc: | $(BUILD_DIR)
	@$(CC) $(CC_FLAGS_2) -o $(TMP_OUT) $(UNIT_TEST)
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

clean: ## Clean up generated files
	@rm -rf $(BUILD_DIR)
	@echo "Cleaned up generated files."

.PHONY: help run unittest warntest errortest clean \
        bootstrap selfhost .run-cc .run-bootstrap .run-selfhost \
        .unittest-cc .unittest-bootstrap .unittest-selfhost \
		.warntest-cc .warntest-bootstrap .warntest-selfhost \
        .errortest-cc .errortest-bootstrap .errortest-selfhost
