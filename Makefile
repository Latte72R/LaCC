CFLAGS=-std=c11 -g -static
SRCS=codegen.c main.c parse.c tokenizer.c
OBJS=$(SRCS:.c=.o)

9cc: $(OBJS)
	$(CC) -o 9cc $(OBJS) $(LDFLAGS)

$(OBJS): 9cc.h

test: 9cc
	./test.sh
	./multitest.sh

rf: 9cc
	./rf.sh $(FILE)

clean:
	rm -f 9cc *.o *~ tmp*

.PHONY: test clean
