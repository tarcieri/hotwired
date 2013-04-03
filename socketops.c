#include <global.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <machdep.h>
#include <socketops.h>

ssize_t read_timeout(int fd, void *buf, size_t nbytes)
{
	fd_set fdset;
	struct timeval tv;

	tv.tv_sec = RESPONSE_TIMEOUT;
	tv.tv_usec = 0;

	FD_ZERO(&fdset);
	FD_SET(fd, &fdset);

	if(select(fd + 1, &fdset, NULL, NULL, &tv) < 1) 
		return -1;

	return recv(fd, buf, nbytes, 0);
}

ssize_t write_timeout(int fd, void *buf, size_t nbytes)
{
	fd_set fdset;
	struct timeval tv;

	tv.tv_sec = RESPONSE_TIMEOUT;
	tv.tv_usec = 0;

	FD_ZERO(&fdset);
	FD_SET(fd, &fdset);

	if(select(fd + 1, NULL, &fdset, NULL, &tv) < 1)
		return -1;

	return send(fd, buf, nbytes, 0);
}

int read_all(int fd, void *buf, size_t nbytes)
{
	ssize_t c;
	uint8_t *bptr = buf;

	while(nbytes > 0) {
		if((c = read_timeout(fd, bptr, nbytes)) < 1)
			return -1;

		nbytes -= c;
		bptr += c;
	}

	return 0;
}

int write_all(int fd, void *buf, size_t nbytes)
{
	ssize_t c;
	uint8_t *bptr = buf;

	while(nbytes > 0) {
		if((c = write_timeout(fd, bptr, nbytes)) < 1)
			return -1;

		nbytes -= c;
		bptr += c;
	}

	return 0;
}

int read_int16(int fd, uint16_t *value)
{
	if(read_all(fd, value, 2) < 0)
		return -1;

#ifndef HOST_BIGENDIAN
	*value = ntohs(*value);
#endif

	return 0;
}

int read_int32(int fd, uint32_t *value)
{
	if(read_all(fd, value, 4) < 0)
		return -1;

#ifndef HOST_BIGENDIAN
	*value = ntohl(*value);
#endif
	
	return 0;
}
