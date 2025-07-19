CFLAGS:=-std=c99 -Wno-incompatible-library-redeclaration -Wno-builtin-declaration-mismatch -Wno-unknown-warning-option
LDFLAGS:=-std=c99
SRC_DIR:=.
TEST_DIR:=./tests
EXAMPLE_DIR:=./examples
BUILD_DIR:=./build
SRCS:=$(SRC_DIR)/main.c $(SRC_DIR)/tokenize.c $(SRC_DIR)/parse.c $(SRC_DIR)/codegen.c
ASMS:=$(SRCS:.c=.s)
BOOSTSTRAP:=$(BUILD_DIR)/lacc
SELFHOST:=$(BUILD_DIR)/laccs

.DEFAULT_GOAL := help
help:
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}'

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
	@echo "Build directory created at $(BUILD_DIR)."

bootstrap: $(BOOSTSTRAP) ## Build the bootstrap compiler
	@echo "Bootstrap compiler built successfully."

selfhost: $(SELFHOST) ## Build the self-hosted compiler
	@echo "Self-hosted compiler built successfully."

define runfile
	@mkdir -p ${BUILD_DIR}
	@$(1) $(2) > ${BUILD_DIR}/tmp.s
	@cc -o ${BUILD_DIR}/tmp ${BUILD_DIR}/tmp.s
	@${BUILD_DIR}/tmp
endef

runfile: $(SELFHOST) ## Run a file with the self-hosted compiler
	$(call runfile, $(SELFHOST), $(FILE))

$(BOOSTSTRAP): $(SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BOOSTSTRAP) $(SRCS) $(SRC_DIR)/extention.c

$(SELFHOST): $(BOOSTSTRAP) | $(BUILD_DIR)
	$(BOOSTSTRAP) $(SRC_DIR)/main.c > $(BUILD_DIR)/main.s
	$(BOOSTSTRAP) $(SRC_DIR)/tokenize.c > $(BUILD_DIR)/tokenize.s
	$(BOOSTSTRAP) $(SRC_DIR)/parse.c > $(BUILD_DIR)/parse.s
	$(BOOSTSTRAP) $(SRC_DIR)/codegen.c > $(BUILD_DIR)/codegen.s
	$(CC) -o $(SELFHOST) $(BUILD_DIR)/main.s $(BUILD_DIR)/tokenize.s $(BUILD_DIR)/parse.s $(BUILD_DIR)/codegen.s $(SRC_DIR)/extention.c $(LDFLAGS)

cc-test: $(BUILD_DIR) ## Run tests with the default C compiler
	@echo \[unitests.c\]
	@cc -w  -o $(BUILD_DIR)/tmp $(TEST_DIR)/unitests.c $(CFLAGS)
	@./$(BUILD_DIR)/tmp
	@echo \[prime.c\]
	@cc -w  -o $(BUILD_DIR)/tmp $(TEST_DIR)/prime.c $(CFLAGS)
	@./$(BUILD_DIR)/tmp
	@echo \[fizzbuzz.c\]
	@cc -w  -o $(BUILD_DIR)/tmp $(TEST_DIR)/fizzbuzz.c $(CFLAGS)
	@./$(BUILD_DIR)/tmp

define multitest
	@echo \[unitests.c\]
	$(call runfile, $(1), $(TEST_DIR)/unitests.c)
	@echo \[prime.c\]
	$(call runfile, $(1), $(TEST_DIR)/prime.c)
	@echo \[fizzbuzz.c\]
	$(call runfile, $(1), $(TEST_DIR)/fizzbuzz.c)
endef

bootstrap-test: $(BOOSTSTRAP) ## Run tests with the bootstrap compiler
	$(call multitest, $(BOOSTSTRAP))

selfhost-test: $(SELFHOST) ## Run tests with the self-hosted compiler
	$(call multitest, $(SELFHOST))

error-test: $(SELFHOST) ## Run error tests with the self-hosted compiler
	$(TEST_DIR)/error_test.sh $(BUILD_DIR) $(SELFHOST)

clean: ## Clean up generated files
	rm -f ./$(BOOSTSTRAP) $(SELFHOST) $(ASMS) $(BUILD_DIR)/tmp.s $(BUILD_DIR)/tmp

.PHONY: bootstrap-test selfhost-test lifegame rotate clean cc-test help runfile
