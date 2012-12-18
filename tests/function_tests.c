#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <sched.h>

#include "../cometbuf.h"

/*
	Write
	Read
	Read fast
	Write fast
	Write to end
	Read to end
	Write/Read random speed
	Threads
	Dump to file
	Load dump file
*/

void main ()
{
	char devfile[] = "/tmp/comlog-shm-XXXXXX";

	printf("Starting tests...\n");

	//cbd_t cb_open(int length, char *template, unsigned int oflag)
	cbd_t cbd;
	cbd = cb_open(4096 * 128, devfile, 0);
	if (cbd > 0) {
		printf("cb_open: ok\n");
	}

	/* store something in it */
	int fd = fopen("tests/testdata-01", "r");
	if (fread((void *)cb_head_addr(cbd), 100, 1, fd) < 0) {
		perror("fread");
	} else {
		if (cb_head_adv(cbd, 100) < 0) {
			printf("head_adv: failed\n");
		}
	}
	
	printf("used bytes: \t%d\n", cb_used_bytes(cbd));
	printf("unused bytes: \t%d\n", cb_unused_bytes(cbd));
	
	/* read from the buffer */
	char tmpbuf[100];
	if (memcpy(tmpbuf, cb_tail_addr(cbd), 100) < 0) {
		perror("memcpy");
	}
	tmpbuf[100] = '\0';
	printf("tmpbuf:\n%s\n", tmpbuf);

	if (cb_free(cbd) < 0) {
		printf("cb_free: failed\n");
	} else {
		printf("cb_free: ok\n");
	}


	return;
}
