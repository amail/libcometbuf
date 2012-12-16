cometbuf:
	gcc -c -fpic cometbuf.c
	gcc -shared -o libcometbuf.so cometbuf.o
