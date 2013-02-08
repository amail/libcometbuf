#ifndef COMETBUF_H_INCLUDED_
#define COMETBUF_H_INCLUDED_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <sys/mman.h>

typedef void * cbd_t;
typedef struct cb_attr cb_attr;

#define CB_FIXED 1
#define CB_LOCKED 2
#define CB_PERSISTANT 4

extern cbd_t cb_open(int length, char *path, unsigned int oflag);
extern int cb_close(cbd_t cbdes);
extern int cb_clear(cbd_t cbdes);
extern void *cb_head_addr(cbd_t cbdes);
extern void *cb_tail_addr(cbd_t cbdes);
extern int cb_head_adv(cbd_t cbdes, unsigned long bytes);
extern int cb_tail_adv(cbd_t cbdes, unsigned long bytes);
extern unsigned long cb_used_bytes(cbd_t cbdes);
extern unsigned long cb_unused_bytes(cbd_t cbdes);
extern int cb_sync(cbd_t cbdes);
extern int cb_buffer_length(cbd_t cbdes);

static unsigned long cb_block_size(char *path);

#endif /* COMETBUF_H_INCLUDED_ */
