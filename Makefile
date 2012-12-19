CC=gcc
CFLAGS=-O3 -s -c -fpic -pthread
LIBS=-lc -lrt

cometbuf:
	$(CC) $(CFLAGS) cometbuf.c $(LIBS)
	$(CC) -shared -o libcometbuf.so cometbuf.o

tests:
	$(MAKE) -C tests
