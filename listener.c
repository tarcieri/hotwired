#include <global.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>

#include <listener.h>

#define MAX_BACKLOG 256

int listener_create(uint32_t source_addr, int port)
{
	int fd, val = 1;
	struct sockaddr_in sin;

	memset(&sin, 0, sizeof(struct sockaddr_in));

	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = source_addr;

	if((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0) {
		perror("socket");
		return -1;
	}

	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
		perror("setsockopt");
		return -1;
	}

	if(bind(fd, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) < 0) {
		perror("bind");
		return -1;
	}

	if(listen(fd, MAX_BACKLOG) < 0) {
		perror("listen");
		return -1;
	}

	return fd;
}

int listener_accept(int fd, uint32_t *addr)
{
	int cfd;
	struct sockaddr_in sin;
	unsigned int len = sizeof(struct sockaddr_in);

	if((cfd = accept(fd, (struct sockaddr *)&sin, &len)) < 0)
		return -1;

	if(addr != NULL)
		*addr = sin.sin_addr.s_addr;

	return cfd;
}
