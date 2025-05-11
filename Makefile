CFLAGS=-std=c11 -g -static -Wno-incompatible-library-redeclaration
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

self: lccs
	./multitest.sh ./lccs

.PHONY: test clean
