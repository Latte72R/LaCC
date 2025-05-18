CFLAGS:=-std=c99 -Wno-incompatible-library-redeclaration -Wno-builtin-declaration-mismatch -Wno-unknown-warning-option
LDFLAGS:=-std=c99
SRCS:=main.c tokenize.c parse.c codegen.c
ASMS:=$(SRCS:.c=.s)
BOOSTSTRAP:=./lacc
SELFHOST:=./laccs

$(BOOSTSTRAP): $(SRCS)
	$(CC) $(CFLAGS) -o $(BOOSTSTRAP) $(SRCS) extention.c

$(SELFHOST): $(BOOSTSTRAP)
	$(BOOSTSTRAP) ./main.c > main.s
	$(BOOSTSTRAP) ./tokenize.c > tokenize.s
	$(BOOSTSTRAP) ./parse.c > parse.s
	$(BOOSTSTRAP) ./codegen.c > codegen.s
	$(CC) -o $(SELFHOST) $(ASMS) extention.c $(LDFLAGS)

clean:
	rm -f $(BOOSTSTRAP) $(SELFHOST) *.o *.s tmp*

test: $(BOOSTSTRAP)
	./multitest.sh $(BOOSTSTRAP)

selfhost-test: $(SELFHOST)
	./multitest.sh $(SELFHOST)

lifegame: $(SELFHOST)
	./rf.sh $(SELFHOST) ./lifegame.c

rotate: $(SELFHOST)
	./rf.sh $(SELFHOST) ./rotate.c

.PHONY: test selfhost-test lifegame rotate clean
