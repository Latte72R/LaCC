CFLAGS=-std=c99 -Wno-incompatible-library-redeclaration -Wno-builtin-declaration-mismatch -Wno-unknown-warning-option
LDFLAGS=-std=c99
SRCS=main.c tokenize.c parse.c codegen.c extention.c

lacc: $(SRCS)
	$(CC) -o lacc $(SRCS) $(CFLAGS)

selfhost: lacc
	./lacc ./main.c > main.s
	./lacc ./tokenize.c > tokenize.s
	./lacc ./parse.c > parse.s
	./lacc ./codegen.c > codegen.s
	$(CC) -o laccs main.s tokenize.s parse.s codegen.s extention.c $(LDFLAGS)

clean:
	rm -f lacc laccs *.o *~ tmp*

test: lacc
	./multitest.sh ./lacc

selfhost-test: selfhost
	./multitest.sh ./laccs

.PHONY: test selfhost-test clean
