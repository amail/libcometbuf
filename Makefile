CC=gcc
CFLAGS=-O3 -s -c -fPIC -pthread
LIBS=-lc -lrt

cometbuf:
	$(CC) $(CFLAGS) $(LIBS) -o cometbuf.o 
	$(CC) -shared -Wl,-soname,libcometbuf.so -o libcometbuf.so.0.1.0 cometbuf.o

tests:
	$(MAKE) -C tests
