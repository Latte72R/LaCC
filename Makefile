CC_FLAGS:=-std=c99 -I include -Wno-incompatible-library-redeclaration -Wno-builtin-declaration-mismatch -Wno-unknown-warning-option -g
LACC_FLAGS:=-I include
SRC_DIR:=./src
TEST_DIR:=./tests
EXAMPLE_DIR:=./examples
BUILD_DIR:=./build
SRCS:=$(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/**/*.c)
ASMS:=$(SRCS:.c=.s)
CC:=cc
BOOSTSTRAP:=$(BUILD_DIR)/lacc
SELFHOST:=$(BUILD_DIR)/laccs

.DEFAULT_GOAL := help
help:
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}'

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
	@echo "Build directory created at $(BUILD_DIR)."

.bootstrap: $(BOOSTSTRAP)

selfhost: $(SELFHOST) ## Build the self-hosted compiler

define runfile
	@mkdir -p ${BUILD_DIR}
	@$(1) $(LACC_FLAGS) $(2) > ${BUILD_DIR}/tmp.s
	@cc -o ${BUILD_DIR}/tmp ${BUILD_DIR}/tmp.s
	@${BUILD_DIR}/tmp
endef

run: .run-selfhost ## Run a file with the self-hosted compiler

.run-cc: $(SELFHOST)
	@$(call runfile, $(CC), $(FILE))
	
.run-bootstrap: $(BOOSTSTRAP)
	@$(call runfile, $(BOOSTSTRAP), $(FILE))

.run-selfhost: $(SELFHOST)
	@$(call runfile, $(SELFHOST), $(FILE))

$(BOOSTSTRAP): $(SRCS) | $(BUILD_DIR)
	@$(CC) $(CC_FLAGS) -o $(BOOSTSTRAP) $(SRCS) extension.c
	@echo "Bootstrap compiler created at $(BOOSTSTRAP)."

$(SELFHOST): $(BOOSTSTRAP) | $(BUILD_DIR)
	@for src in $(SRCS); do \
		base=$$(basename $$src .c); \
		out=$(BUILD_DIR)/$$base.s; \
		$(BOOSTSTRAP) $(LACC_FLAGS) $$src > $$out; \
	done
	@$(CC) -o $(SELFHOST) $(BUILD_DIR)/*.s extension.c $(CC_FLAGS)
	@echo "Self-hosted compiler created at $(SELFHOST)."

define unittest
	@$(call runfile, $(1), $(TEST_DIR)/unittest.c)
endef

define errortest
	@$(TEST_DIR)/errortest.sh $(BUILD_DIR) $(1)
endef

unittest: .unittest-selfhost ## Run unit tests with the self-hosted compiler

errortest: .errortest-selfhost ## Run error tests with the self-hosted compiler

.unittest-cc:
	@$(call unittest, $(CC))

.unittest-bootstrap: $(BOOSTSTRAP)
	@$(call unittest, $(BOOSTSTRAP))

.unittest-selfhost: $(SELFHOST)
	@$(call unittest, $(SELFHOST))

.errortest-cc:
	@$(call errortest, $(CC))

.errortest-bootstrap: $(BOOSTSTRAP)
	@$(call errortest, $(BOOSTSTRAP))

.errortest-selfhost: $(SELFHOST)
	@$(call errortest, $(SELFHOST))

clean: ## Clean up generated files
	@rm -rf $(BUILD_DIR)
	@echo "Cleaned up generated files."

.PHONY: bootstrap-test selfhost-test lifegame rotate clean cc-test help runfile
