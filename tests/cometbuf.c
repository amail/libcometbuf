#include "cometbuf.h"

struct cb_attr {
	unsigned int tail, head, size;
	unsigned int oflag;
	unsigned long page_size;
	void *address;
};

cbd_t cb_open(int length, char *template, unsigned int oflag)
{
	cb_attr *buffer;
	int mmap_fd, dump_fd;
	unsigned long page_size;
	void *addr;

	/* check page size */
	page_size = sysconf(_SC_PAGESIZE);
	if (page_size <= 0 || length % page_size != 0 || sizeof(cb_attr) > page_size) {
		perror("page size");
		return -1;
	}

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

	if (ftruncate(mmap_fd, length + page_size) < 0) {
		perror("ftruncate");
		return -1;
	}

	/* mmap */
	addr = mmap(NULL, buffer->size + page_size, PROT_WRITE, MAP_FIXED | MAP_SHARED, mmap_fd, 0);
	if (addr < 0) {
		perror("mmap");
		return -1;
	}

	/* set buffer descriptor attributes */
	buffer = addr;
	buffer->address = addr + page_size;
	buffer->size = length;
	buffer->page_size = page_size;
	buffer->oflag = oflag;

	/* use automatic wrap around */
	if (!(CB_FIXED & buffer->oflag)) {
		addr = mmap(buffer->address + buffer->size, buffer->size, PROT_WRITE, MAP_FIXED | MAP_SHARED, mmap_fd, page_size);
		if (addr != buffer->address + buffer->size) {
			perror("mmap");
			return -1;
		}
	}

	/* clean up */
	addr = 0;
	close(mmap_fd);

	/* madvise */
	if (0 < madvise(buffer->address, buffer->size, MADV_SEQUENTIAL)) {
		perror("madvise");
	}

	/* lock memory to avoid swapping*/
	if (CB_LOCKED & buffer->oflag) {
		if (0 < mlockall(MCL_CURRENT | MCL_FUTURE)) {
			perror("mlockall");
		}
	} else {
		if (mlock(buffer, page_size) < 0) {
			perror("mlock");
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

	buffer->head = 0;
	buffer->tail = 0;
	buffer->address = 0;

	munlockall();
	munmap(buffer, buffer->size + buffer->page_size);
	
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

int cb_head_adv(cbd_t cbdes, unsigned long bytes)
{
	cb_attr *buffer = cbdes;

	if (CB_PERSISTANT & buffer->oflag) {
		/* sync buffer body */
		if (msync(buffer->address + buffer->head, bytes, MS_SYNC) < 0) {
			perror("msync buffer");
			return -1;
		}

		/* sync buffer header */
		if (msync(buffer, buffer->page_size, MS_SYNC) < 0) {
			perror("msync header");
			return -1;
		}
	}

	buffer->head += bytes;

	return 0;
}

void cb_tail_adv(cbd_t cbdes, unsigned long bytes)
{
	cb_attr *buffer = cbdes;

	buffer->tail += bytes;	

	if (buffer->tail >= buffer->size) {
		buffer->head -= buffer->size;
		buffer->tail -= buffer->size;
	}
}

unsigned long cb_used_bytes(cbd_t cbdes)
{
	cb_attr *buffer = cbdes;

	return buffer->head - buffer->tail;
}

unsigned long cb_unused_bytes(cbd_t cbdes)
{
	cb_attr *buffer = cbdes;

	return buffer->size - (buffer->head - buffer->tail);
}

int cb_sync(cbd_t cbdes)
{
	cb_attr *buffer = cbdes;

	/* sync buffer body */
	if (msync(buffer->address + buffer->tail, buffer->head - buffer->tail, MS_SYNC) < 0) {
		perror("msync buffer");
		return -1;
	}

	/* sync buffer header */
	if (msync(buffer, buffer->page_size, MS_SYNC) < 0) {
		perror("msync header");
		return -1;
	}

	return 0;
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
