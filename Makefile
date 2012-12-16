CC=gcc
CFLAGS=-O3 -s -c -fpic -pthread -lc -lrt

cometbuf:
	$(CC) $(CFLAGS) cometbuf.c
	$(CC) -shared -o libcometbuf.so cometbuf.o
