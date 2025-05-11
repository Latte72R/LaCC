<<<<<<< HEAD
CFLAGS=-std=c11 -g -static
SRCS=codegen.c main.c parse.c tokenizer.c
OBJS=$(SRCS:.c=.o)

lcc: $(OBJS)
	$(CC) -o lcc $(OBJS) $(LDFLAGS)

$(OBJS): lcc.h

test: lcc
	./multitest.sh
=======
CFLAGS=-std=c11 -g -static -Wno-incompatible-library-redeclaration
SRCS=lcc.c extention.c

lcc: $(SRCS)
	$(CC) -o lcc $(SRCS) $(CFLAGS)

lccs: lcc
	./lcc ./lcc.c > tmp.s
	$(CC) -o lccs tmp.s extention.c $(LDFLAGS)
>>>>>>> cdb0c1a (Mission Accomplished)

clean:
	rm -f lcc *.o *~ tmp*

<<<<<<< HEAD
=======
test: lcc
	./multitest.sh ./lcc

self: lccs
	./multitest.sh ./lccs

>>>>>>> cdb0c1a (Mission Accomplished)
.PHONY: test clean
