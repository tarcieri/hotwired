/* Abstract stateful multiplexer - implemented with kqueues */

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <Multiplexer.h>

static int kq = -1;

void multiplexer_create()
{
	if(kq != -1)
		close(kq);

	if((kq = kqueue()) < 0) {
		perror("kqueue");
		exit(EXIT_FAILURE);
	}
}

void multiplexer_destroy()
{
	close(kq);
}

void multiplexer_add(int fd, mevent_t event)
{
	struct kevent kev;

	switch(event) {
		case MPLX_RD:
			kev.filter = EVFILT_READ;
			break;
		case MPLX_WR:
			kev.filter = EVFILT_WRITE;
			break;
	}
	
	kev.ident = fd;
	kev.flags = EV_ADD | EV_ENABLE | EV_CLEAR;
	kev.fflags = 0;

	kevent(kq, &kev, 1, NULL, 0, NULL);

}

void multiplexer_remove(int fd, mevent_t event)
{
	struct kevent kev;

	switch(event) {
		case MPLX_RD:
			kev.filter = EVFILT_READ;
			break;
		case MPLX_WR:
			kev.filter = EVFILT_WRITE;
			break;
	}
	
	kev.ident = fd;
	kev.flags = EV_DELETE;
	kev.fflags = 0;

	kevent(kq, &kev, 1, NULL, 0, NULL);
}

int multiplexer_poll(int *fd, mevent_t *event)
{
	int evtype = -1;
	struct kevent kev;

	if(kevent(kq, NULL, 0, &kev, 1, NULL) <= 0)
		return -1;

	if(kev.flags & EV_ERROR) 
		return -1;
	else if(kev.filter == EVFILT_READ) 
		evtype = MPLX_RD;
	else if(kev.filter == EVFILT_WRITE)
		evtype = MPLX_WR;

	if(fd != NULL)
		*fd = kev.ident;

	if(event != NULL)
		*event = evtype;

	return 0;
}
