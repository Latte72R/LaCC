CFLAGS:=-std=c99 -Wno-incompatible-library-redeclaration -Wno-builtin-declaration-mismatch -Wno-unknown-warning-option
LDFLAGS:=-std=c99
SRCS:=main.c tokenize.c parse.c codegen.c
ASMS:=$(SRCS:.c=.s)
BOOSTSTRAP:=./lacc
SELFHOST:=./laccs

.DEFAULT_GOAL := help
help:
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}'

bootstrap: $(BOOSTSTRAP) ## Build the bootstrap compiler
	@echo "Bootstrap compiler built successfully."

selfhost: $(SELFHOST) ## Build the self-hosted compiler
	@echo "Self-hosted compiler built successfully."

runfile: $(SELFHOST) ## Run a file with the self-hosted compiler
	./rf.sh $(SELFHOST) $(FILE)

$(BOOSTSTRAP): $(SRCS)
	$(CC) $(CFLAGS) -o $(BOOSTSTRAP) $(SRCS) extention.c

$(SELFHOST): $(BOOSTSTRAP)
	$(BOOSTSTRAP) ./main.c > main.s
	$(BOOSTSTRAP) ./tokenize.c > tokenize.s
	$(BOOSTSTRAP) ./parse.c > parse.s
	$(BOOSTSTRAP) ./codegen.c > codegen.s
	$(CC) -o $(SELFHOST) $(ASMS) extention.c $(LDFLAGS)

cc-test: ## Run tests with the default C compiler
	echo \[unitests.c\]
	cc ./unitests.c $(CFLAGS)
	./a.out
	echo \[prime.c\]
	cc ./prime.c $(CFLAGS)
	./a.out
	echo \[fizzbuzz.c\]
	cc ./fizzbuzz.c $(CFLAGS)
	./a.out

test: $(BOOSTSTRAP) ## Run tests with the bootstrap compiler
	./multitest.sh $(BOOSTSTRAP)

selfhost-test: $(SELFHOST) ## Run tests with the self-hosted compiler
	./multitest.sh $(SELFHOST)

error-test: $(SELFHOST) ## Run error tests with the self-hosted compiler
	./error_test.sh $(SELFHOST)

lifegame: $(SELFHOST) ## Run tests for the life game
	./rf.sh $(SELFHOST) ./lifegame.c

rotate: $(SELFHOST) ## Run tests for the rotate program
	./rf.sh $(SELFHOST) ./rotate.c

clean: ## Clean up generated files
	rm -f $(BOOSTSTRAP) $(SELFHOST) *.o *.s tmp*

.PHONY: test selfhost-test lifegame rotate clean cc-test help
