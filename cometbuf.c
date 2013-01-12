#include "cometbuf.h"

#define ERROR_VAL 0

struct cb_attr {
	unsigned int tail, head, size;
	unsigned int oflag;
	unsigned long page_size;
	void *address;
};

cbd_t cb_open(int length, char *path, unsigned int oflag)
{
	cb_attr *buffer;
	int mmap_fd, zero_fd;
	unsigned long page_size;
	void *addr, *addr_init;
	int file_exists;

	/* check page size */
	page_size = sysconf(_SC_PAGESIZE);
	if (page_size <= 0 || length % page_size != 0 || sizeof(cb_attr) > page_size) {
		perror("page size");
		return ERROR_VAL;
	}

	/* check if file can be accessed */
	file_exists = access(path, W_OK | R_OK);
	if (file_exists < 0) {
		perror("dump file");
		return ERROR_VAL;
	}

	/* open files */
	mmap_fd = open(path, O_RDWR | O_CREAT);
	zero_fd = open("/dev/zero", O_RDWR); 

	if (mmap_fd < 0 || zero_fd < 0) {
		perror("open");
		return ERROR_VAL;
	}

	/* persistance */
	if (!(CB_PERSISTANT & buffer->oflag) || file_exists < 0) {
		if (ftruncate(mmap_fd, length + page_size) < 0) {
			perror("ftruncate");
			return ERROR_VAL;
		}
	}

	/* initial mmap to allocate a big enough mem area */
	addr_init = mmap(NULL, length * 2 + page_size, PROT_READ | PROT_WRITE, MAP_SHARED, zero_fd, 0);
	if (addr_init < 1) {
		perror("mmap");
		return ERROR_VAL;
	}

	/* mmap */
	buffer = mmap(addr_init, length + page_size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, mmap_fd, 0);
	if (buffer != addr_init) {
		perror("mmap");
		return ERROR_VAL;
	}

	/* use automatic wrap around */
	if (!(CB_FIXED & oflag)) {
		addr = mmap(addr_init + page_size + length, length, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, mmap_fd, page_size);
		if (addr != addr_init + page_size + length) {
			perror("mmap");
			return ERROR_VAL;
		}
	}

	/* clean up */
	close(mmap_fd);
	close(zero_fd);

	/* set buffer descriptor attributes */
	buffer->address = buffer + page_size;
	if (CB_PERSISTANT & buffer->oflag || file_exists >= 0) {
		buffer->size = length;
		buffer->page_size = page_size;
		buffer->oflag = oflag;
	} else {
		buffer->tail = 0;
		buffer->head = 0;
	}

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

	return (cbd_t) buffer;
}

int cb_free(cbd_t cbdes)
{
	cb_attr *buffer = (cb_attr *) cbdes;

	munmap(buffer, buffer->size + buffer->page_size);

	return 0;
}

int cb_clear(cbd_t cbdes)
{
	cb_attr *buffer = (cb_attr *) cbdes;

	/* clear memory */
	if (buffer->address != memset(buffer->address, 0, buffer->size)) {
		perror("memset");
		return ERROR_VAL;
	}

	buffer->head = 0;
	buffer->tail = 0;

	return 0;
}

void *cb_head_addr(cbd_t cbdes)
{
	cb_attr *buffer = (cb_attr *) cbdes;

	return (void *) (buffer->address + buffer->head);	
}

void *cb_tail_addr(cbd_t cbdes)
{
	cb_attr *buffer = (cb_attr *) cbdes;

	return (void *) (buffer->address + buffer->tail);	
}

int cb_head_adv(cbd_t cbdes, unsigned long bytes)
{
	cb_attr *buffer = (cb_attr *) cbdes;

	if (CB_PERSISTANT & buffer->oflag) {
		/* sync buffer */
		unsigned long offset  = (buffer->head / buffer->page_size) * buffer->page_size;
		unsigned long pages = ((bytes / buffer->page_size) + 1) * buffer->page_size;

		if (msync(buffer->address + offset, pages, MS_SYNC) < 0) {
			perror("msync buffer");
			return ERROR_VAL;
		}

		/* advance pointer */
		buffer->head += bytes;

		/* sync buffer header */
		if (munlock(buffer, buffer->page_size) < 0) {
			perror("munlock header");
		}
		if (msync(buffer, buffer->page_size, MS_SYNC) < 0) {
			perror("msync header");
			return ERROR_VAL;
		}
		if (mlock(buffer, buffer->page_size) < 0) {
			perror("mlock header");
		}
	} else {
		/* advance pointer */
		buffer->head += bytes;
	}

	return buffer->head;
}

int cb_tail_adv(cbd_t cbdes, unsigned long bytes)
{
	cb_attr *buffer = (cb_attr *) cbdes;

	buffer->tail += bytes;	

	if (buffer->tail >= buffer->size) {
		buffer->head -= buffer->size;
		buffer->tail -= buffer->size;
	}

	if (CB_PERSISTANT & buffer->oflag) {
		/* sync buffer header */
		if (munlock(buffer, buffer->page_size) < 0) {
			perror("munlock header");
		}
		if (msync(buffer, buffer->page_size, MS_SYNC) < 0) {
			perror("msync header");
			return ERROR_VAL;
		}
		if (mlock(buffer, buffer->page_size) < 0) {
			perror("mlock header");
		}
	}

	return 0;
}

unsigned long cb_used_bytes(cbd_t cbdes)
{
	cb_attr *buffer = (cb_attr *) cbdes;

	return buffer->head - buffer->tail;
}

unsigned long cb_unused_bytes(cbd_t cbdes)
{
	cb_attr *buffer = (cb_attr *) cbdes;

	return buffer->size - (buffer->head - buffer->tail);
}

int cb_sync(cbd_t cbdes)
{
	cb_attr *buffer = (cb_attr *) cbdes;

	/* sync buffer */
	unsigned long offset  = (buffer->tail / buffer->page_size) * buffer->page_size;
	unsigned long pages = (((buffer->head - buffer->tail) / buffer->page_size) + 1) * buffer->page_size;

	if (msync(buffer->address + offset, pages, MS_SYNC) < 0) {
		perror("msync buffer");
		return ERROR_VAL;
	}

	/* sync buffer header */
	if (munlock(buffer, buffer->page_size) < 0) {
		perror("munlock header");
	}
	if (msync(buffer, buffer->page_size, MS_SYNC) < 0) {
		perror("msync header");
		return ERROR_VAL;
	}
	if (mlock(buffer, buffer->page_size) < 0) {
		perror("mlock header");
	}

	return 0;
}

int cb_buffer_length(cbd_t cbdes)
{
	cb_attr *buffer = (cb_attr *) cbdes;

	return (int) buffer->size;
}

static unsigned long cb_block_size(char *path)
{
	struct statvfs st;
	if (statvfs(path, &st) < 0) {
		perror("statvfs");	
		return ERROR_VAL;
	}
	
	return (unsigned long)st.f_bsize;
}
