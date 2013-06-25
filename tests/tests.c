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

struct cb_attr {
	unsigned int tail, head, size;
	unsigned int oflag;
	unsigned long page_size;
	void *address;
};

/*
	Basics (open, write, read, clear and free)
	Fill (open and write to full)
	Read to end
	Wrap around (write, advance and read)
	Overflow (write)
	Performance test (read and write)
	Threading (read and write in different speeds)
	Try to break (write and read various data and sequences)
*/

void print_header(const char *title)
{
	printf("================================================================================\n");
	printf(" %s\n", title);
	printf("================================================================================\n");
}

void print_info(cbd_t cbd)
{
	cb_attr *buffer = cbd;

	printf(" * size (bytes):\t\t%d (%d * %d + %d)\n",
		buffer->size, buffer->page_size,
		buffer->size / buffer->page_size,
		buffer->size % buffer->page_size);
	if (buffer->size / buffer->page_size > 0 && buffer->size % buffer->page_size == 0) {
		printf(" * size (valid):\t\t[ OK ]\n");
	} else {
		printf(" * size (valid):\t\t[ NO ]\n");
	}
	printf(" * address:\t\t\t0x%x\n", buffer->address);
	printf(" * page size:\t\t\t%d\n", buffer->page_size);
	printf(" * oflags:\t\t\t%d\n", buffer->oflag);

}

void print_stats(cbd_t cbd)
{
	cb_attr *buffer = cbd;

	if (cb_unused_bytes(cbd) + cb_used_bytes(cbd) == buffer->size) {
		printf(" * valid:\t\t\t[ OK ] (%d/%d)\n", cb_used_bytes(cbd), cb_unused_bytes(cbd));
	} else {
		printf(" * valid:\t\t\t[ NO ]\n");
	}
}

void print_address(cbd_t cbd)
{
	cb_attr *buffer = cbd;

	printf(" * tail address:\t\t%x (offset: %d, %d)\n",
		cb_tail_addr(cbd),
		cb_tail_addr(cbd) - buffer->address,
		buffer->address + buffer->size - (unsigned int) cb_tail_addr(cbd));
	printf(" * head address:\t\t%x (offset: %d, %d)\n",
		cb_head_addr(cbd),
		cb_head_addr(cbd) - buffer->address,
		buffer->address + buffer->size - (unsigned int) cb_head_addr(cbd));
}

int main ()
{	
	unsigned long int page_size, dummy_size;
	cbd_t cbd;
	int fd;
	char *dummy;

	/* settings */	
	page_size = sysconf(_SC_PAGESIZE);

	print_header("test 01: open, write, clear and free a buffer");
	/* dummy data 01 */
	fd = fopen("tests/testdata-01", "rb");
	fseek(fd, 0L, SEEK_END);
	dummy_size = ftell(fd);
	rewind(fd);
	dummy = malloc(dummy_size);

	/* try open, write, clear and free */
	cbd = cb_open(page_size * 128, "/tmp/cometbuf.dump", 0);
	if (cbd < 0) {
		printf(" * open /tmp/cometbuf.dump\t[ NO ]\n");
		return -1;
	}
	printf(" * open /tmp/cometbuf.dump\t[ OK ]\n");

	print_info(cbd);

	/* put all the dummy data in it */
	printf(" * writing %d bytes...\n", dummy_size);
	if (fread(cb_head_addr(cbd), dummy_size, 1, fd) < 0) {
		perror("fread");
		return -1;
	} else {
		if (cb_head_adv(cbd, dummy_size) < 0) {
			printf("head_adv: failed\n");
		}
	}
	
	print_stats(cbd);
	
	/* read from the buffer */
	if (memcpy(dummy, cb_tail_addr(cbd), dummy_size) < 0) {
		perror("memcpy");
	}

	/* compare the data */
	if (0 > bcmp(dummy, cb_tail_addr(cbd), dummy_size)) {
		printf(" * buffer data\t\t\t[ NO ]\n");
	} else {
		printf(" * buffer data\t\t\t[ OK ]\n");
	}

	/* advance pointer */
	if (0 > cb_tail_adv(cbd, dummy_size)) {
		printf(" * tail adv:\t\t\t[ NO ]\n");
	} else {
		printf(" * tail adv:\t\t\t[ OK ]\n");
	}

	/* clear buffer */
	if (cb_clear(cbd) < 0) {
		printf(" * clear:\t\t\t[ NO ]\n");
	} else {
		printf(" * clear:\t\t\t[ OK ]\n");
	}

	/* free buffer */
	if (cb_close(cbd) < 0) {
		printf(" * free:\t\t\t[ NO ]\n");
	} else {
		printf(" * free:\t\t\t[ OK ]\n");
	}

	/* cleanup */
	fclose(fd);
	free(dummy);


	print_header("test 02: open and fill a buffer");
	/* dummy data 02 */
	fd = fopen("tests/testdata-02", "rb");
	dummy_size = page_size * 16;
	dummy = malloc(dummy_size);

	/* open */
	cbd = cb_open(dummy_size, "/tmp/cometbuf2.dump", 0);
	if (cbd < 0) {
		printf(" * open /tmp/cometbuf2.dump\t[ NO ]\n");
		return -1;
	}
	printf(" * open /tmp/cometbuf2.dump\t[ OK ]\n");

	print_info(cbd);

	/* put all the dummy data in it */
	printf(" * writing %d bytes...\n", dummy_size);
	if (fread(cb_head_addr(cbd), dummy_size, 1, fd) < 0) {
		perror("fread");
		return -1;
	} else {
		if (cb_head_adv(cbd, dummy_size) < 0) {
			printf("head_adv: failed\n");
		}
	}

	print_stats(cbd);
	print_address(cbd);

	/* read from the buffer */
	if (memcpy(dummy, cb_tail_addr(cbd), dummy_size) < 0) {
		perror("memcpy");
	}

	/* compare the data */
	if (0 > bcmp(dummy, cb_tail_addr(cbd), dummy_size)) {
		printf(" * buffer data\t\t\t[ NO ]\n");
	} else {
		printf(" * buffer data\t\t\t[ OK ]\n");
	}
	
	/* cleanup */
	fclose(fd);
	free(dummy);
	cb_close(cbd);


	print_header("test 03: persistance");

	/* open */
	cbd = cb_open(dummy_size, "/tmp/cometbuf.dump", CB_PERSISTANT);
	if (cbd < 0) {
		printf(" * open /tmp/cometbuf.dump\t[ NO ]\n");
		return -1;
	}
	printf(" * open /tmp/cometbuf.dump\t[ OK ]\n");
	print_info(cbd);

	/* add data */
	printf(" * writing 10 bytes...\n");
	if (memcpy(cb_head_addr(cbd), "123456789\0", 10) < 0) {
		perror("memcpy");
		return -1;
	}
	
	if (cb_head_adv(cbd, 10) < 0) {
		printf("head_adv: failed\n");
	}
	
	print_stats(cbd);

	/* add data again */
	printf(" * writing 10 bytes...\n");
	if (memcpy(cb_head_addr(cbd), "123456789\0", 10) < 0) {
		perror("memcpy");
		return -1;
	}
	
	if (cb_head_adv(cbd, 10) < 0) {
		printf("head_adv: failed\n");
	}
	
	print_stats(cbd);

	/* close it */
	printf(" * closing /tmp/cometbuf.dump\n");
	if (cb_close(cbd) < 0) {
		perror("cb_close");
		return -1;
	}

	/* reopen it */
	cbd = cb_open(dummy_size, "/tmp/cometbuf.dump", CB_PERSISTANT);
	if (cbd < 0) {
		printf(" * reopen /tmp/cometbuf.dump\t[ NO ]\n");
		return -1;
	}
	printf(" * reopen /tmp/cometbuf.dump\t[ OK ]\n");
	print_stats(cbd);

	if (bcmp("123456789\0", cb_tail_addr(cbd), 10)) {
		printf(" * buffer data\t\t\t[ NO ]\n");
		return -1;
	}
	if (cb_used_bytes(cbd) != 20) {
		printf(" * buffer data\t\t\t[ NO ]\n");
		return -1;
	}
	printf(" * buffer data\t\t\t[ OK ]\n");

	
	printf(" test\t[ OK ]\n");
	return;
}
