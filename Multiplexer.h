#ifndef MULTIPLEXER_H
#define MULTIPLEXER_H

#include <pthread.h>

typedef enum {
	MPLX_RD,
	MPLX_WR
} mevent_t;

void multiplexer_create();
void multiplexer_destroy();

void multiplexer_add(int fd, mevent_t event);
void multiplexer_remove(int fd, mevent_t event);
int multiplexer_poll(int *fd, mevent_t *event);
	
#endif
