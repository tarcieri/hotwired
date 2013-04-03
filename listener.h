#ifndef LISTENER_H
#define LISTENER_H

int listener_create(uint32_t source_addr, int port);
int listener_accept(int fd, uint32_t *addr);
	
#endif
