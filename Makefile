CFLAGS=-std=c99 -Wno-incompatible-library-redeclaration -Wno-builtin-declaration-mismatch -Wno-unknown-warning-option
LDFLAGS=-std=c99
SRCS=lcc.c extention.c

lcc: $(SRCS)
	$(CC) -o lcc $(SRCS) $(CFLAGS)

lccs: lcc
	./lcc ./lcc.c > tmp.s
	$(CC) -o lccs tmp.s extention.c $(LDFLAGS)

clean:
	rm -f lcc *.o *~ tmp*

test: lcc
	./multitest.sh ./lcc

selfhost-test: lccs
	./multitest.sh ./lccs

.PHONY: test selfhost-test clean
