#ifndef HELPER_THREAD_H
#define HELPER_THREAD_H

#include <global.h>

typedef struct _ConnectionInfo {
	int fd;
	uint32_t ip;
} *ConnectionInfo;

void *helper_thread(void *queue);

#endif
