CC=gcc
CFLAGS=-O3 -s -c -fPIC -pthread
LIBS=-lc -lrt
VERSION=0.2.0

cometbuf:
	$(CC) $(CFLAGS) $(LIBS) -o cometbuf.o cometbuf.c
	$(CC) -shared -Wl,-soname,libcometbuf.so -o libcometbuf.so.$(VERSION) cometbuf.o

tests:
	$(MAKE) -C tests

install:
	rm -f /usr/local/lib/libcometbuf.so
	cp libcometbuf.so.$(VERSION) /usr/local/lib/
	chmod 0755 /usr/local/lib/libcometbuf.so.$(VERSION)
	ldconfig -n /usr/local/lib
	cp cometbuf.h /usr/local/include/
