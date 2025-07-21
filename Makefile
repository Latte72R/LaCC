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

bootstrap: $(BOOSTSTRAP) ## Build the bootstrap compiler

selfhost: $(SELFHOST) ## Build the self-hosted compiler

define runfile
	@mkdir -p ${BUILD_DIR}
	@$(1) $(LACC_FLAGS) $(2) > ${BUILD_DIR}/tmp.s
	@cc -o ${BUILD_DIR}/tmp ${BUILD_DIR}/tmp.s
	@${BUILD_DIR}/tmp
endef

runfile: $(SELFHOST) ## Run a file with the self-hosted compiler
	$(call runfile, $(SELFHOST), $(FILE))

$(BOOSTSTRAP): $(SRCS) | $(BUILD_DIR)
	@$(CC) $(CC_FLAGS) -o $(BOOSTSTRAP) $(SRCS) extention.c
	@echo "Bootstrap compiler created at $(BOOSTSTRAP)."

$(SELFHOST): $(BOOSTSTRAP) | $(BUILD_DIR)
	@for src in $(SRCS); do \
		base=$$(basename $$src .c); \
		out=$(BUILD_DIR)/$$base.s; \
		$(BOOSTSTRAP) $(LACC_FLAGS) $$src > $$out; \
	done
	@$(CC) -o $(SELFHOST) $(BUILD_DIR)/*.s extention.c $(CC_FLAGS)
	@echo "Self-hosted compiler created at $(SELFHOST)."

define unittests
	$(call runfile, $(1), $(TEST_DIR)/unitests.c)
endef

define errortests
	@$(TEST_DIR)/errortests.sh $(BUILD_DIR) $(1)
endef

unittests: .unittests-selfhost ## Run unit tests with the self-hosted compiler

errortests: .errortests-selfhost ## Run error tests with the self-hosted compiler

.unittests-cc:
	$(call unittests, $(CC))

.unittests-bootstrap: $(BOOSTSTRAP)
	$(call unittests, $(BOOSTSTRAP))

.unittests-selfhost: $(SELFHOST)
	$(call unittests, $(SELFHOST))

.errortests-cc:
	$(call errortests, $(CC))

.errortests-bootstrap: $(BOOSTSTRAP)
	$(call errortests, $(BOOSTSTRAP))

.errortests-selfhost: $(SELFHOST)
	$(call errortests, $(SELFHOST))

clean: ## Clean up generated files
	@rm -rf $(BUILD_DIR)
	@echo "Cleaned up generated files."

.PHONY: bootstrap-test selfhost-test lifegame rotate clean cc-test help runfile
