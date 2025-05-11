CFLAGS=-std=c11 -g -static
SRCS=codegen.c main.c parse.c tokenizer.c
OBJS=$(SRCS:.c=.o)

lcc: $(OBJS)
	$(CC) -o lcc $(OBJS) $(LDFLAGS)

$(OBJS): lcc.h

test: lcc
	./multitest.sh

clean:
	rm -f lcc *.o *~ tmp*

.PHONY: test clean
