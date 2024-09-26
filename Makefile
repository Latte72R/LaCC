CFLAGS=-std=c11 -g -static

lcc: lcc.c

test: lcc
	./test.sh

clean:
	rm -f lcc *.o *~ tmp*

.PHONY: test clean