CFLAGS=-std=c99 -Wno-incompatible-library-redeclaration -Wno-builtin-declaration-mismatch -Wno-unknown-warning-option
LDFLAGS=-std=c99
SRCS=main.c tokenize.c parse.c codegen.c extention.c

lcc: $(SRCS)
	$(CC) -o lcc $(SRCS) $(CFLAGS)

lccs: lcc
	./lcc ./main.c > main.s
	./lcc ./tokenize.c > tokenize.s
	./lcc ./parse.c > parse.s
	./lcc ./codegen.c > codegen.s
	$(CC) -o lccs main.s tokenize.s parse.s codegen.s extention.c $(LDFLAGS)

clean:
	rm -f lcc lccs *.o *~ tmp*

test: lcc
	./multitest.sh ./lcc

selfhost-test: lccs
	./multitest.sh ./lccs

.PHONY: test selfhost-test clean
