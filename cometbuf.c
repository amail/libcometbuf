#include "cometbuf.h"

struct cb_attr {
	unsigned int tail, head, size;
	unsigned int oflag;
	void *address;
};

cbd_t cb_open(int size, char *template, unsigned int oflag)
{
	cb_attr *buffer;
	int mmap_fd, dump_fd;
	unsigned long block_size;
	void *addr;

	/* create temp file */
	mmap_fd = mkstemp(template);
	if (mmap_fd < 0) {
		perror("mkstemp");
		return -1;
	}

	if (unlink(template) < 0) {
		perror("unlink");
		return -1;
	}

	if (ftruncate(mmap_fd, buffer->size) < 0) {
		perror("ftruncate");
		return -1;
	}

	/* check requested size with block size */
	block_size = cb_block_size(template);
	if (block_size <= 0 || size % block_size != 0) {
		perror("block size");
		return -1;
	}

	/* allocate buffer descriptor */
	buffer = valloc(sizeof(cb_attr));
	if (buffer <= 0) {
		perror("valloc");
		return -1;
	}
	if (buffer != memset(buffer, 0, sizeof(cb_attr))) {
		perror("memset");
		return -1;
	}
	buffer->size = size;
	buffer->oflag = oflag;

	/* mmap */
	buffer->address = mmap(NULL, buffer->size, PROT_WRITE, MAP_FIXED | MAP_SHARED, mmap_fd, 0);
	if (buffer->address < 0) {
		perror("mmap");
		return -1;
	}

	/* use automatic wrapaound */
	if (!(CB_FIXED & oflag)) {
		addr = mmap(buffer->address + buffer->size, buffer->size, PROT_WRITE, MAP_FIXED | MAP_SHARED, mmap_fd, 0);
		if (addr != buffer->address + buffer->size) {
			perror("mmap");
			return -1;
		}
	}

	close(mmap_fd);

	/* clear memory */
	if (buffer->address != memset(buffer->address, 0, buffer->size)) {
		perror("memset");
		return -1;
	}

	/* madvise */
	if (0 < madvise(buffer->address, buffer->size, MADV_SEQUENTIAL)) {
		perror("madvice");
	}

	/* lock memory to avoid swapping*/
	if (CB_LOCKED & oflag) {
		if (0 < mlockall(MCL_CURRENT | MCL_FUTURE)) {
			perror("mlockall");
		}
	}

	/* set tail and head pointers */
	buffer->tail = 0;
	buffer->head = 0;

	return (cbd_t) buffer;
}

int cb_free(cbd_t cbdes)
{
	cb_attr *buffer = cbdes;

	/* clear memory */
	if (buffer->address != memset(buffer->address, 0, buffer->size)) {
		perror("memset");
	}

	munlockall();
	munmap(buffer->address, buffer->size);
	
	free(buffer);

	return 0;
}

void *cb_head_addr(cbd_t cbdes)
{
	cb_attr *buffer = cbdes;

	return buffer->address + buffer->head;	
}

void *cb_tail_addr(cbd_t cbdes)
{
	cb_attr *buffer = cbdes;

	return buffer->address + buffer->tail;	
}

void cb_head_adv(cbd_t cbdes, unsigned long count)
{
	cb_attr *buffer = cbdes;

	buffer->head += count;
}

void cb_tail_adv(cbd_t cbdes, unsigned long count)
{
	cb_attr *buffer = cbdes;

	buffer->tail += count;	

	if (buffer->tail >= buffer->size) {
		buffer->head -= buffer->size;
		buffer->tail -= buffer->size;
	}
}

unsigned long cb_available_bytes(cbd_t cbdes)
{
	cb_attr *buffer = cbdes;

	return buffer->size - (buffer->head - buffer->tail);
}

static unsigned long cb_block_size(char *path)
{
	struct statvfs st;
	if (statvfs(path, &st) < 0) {
		perror("statvfs");	
		return -1;
	}
	
	return (unsigned long)st.f_bsize;
}
