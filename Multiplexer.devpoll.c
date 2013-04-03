/* Abstract stateful multiplexer - implemented using /dev/poll */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/devpoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <stropts.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <pthread.h>

#include <Multiplexer.h>
#include <RCL.h>
#include <machdep.h>

#ifdef DEBUG
#include <output.h>
#endif

static int dp = -1;
static pthread_t mtid;
static RCL mlock = NULL;

static void multiplex_handler(int i)
{
#ifdef DEBUG
	debug("*** Multiplex notifier activated");
#endif
}

void multiplexer_create()
{
	mtid = pthread_self();

	if(dp != -1)
		close(dp);

	if(mlock == NULL)
		mlock = rcl_create();

	if((dp = open("/dev/poll", O_RDWR)) < 0) {
		perror("/dev/poll");
		exit(EXIT_FAILURE);
	}

#ifdef HAVE_SIGSET
	sigset(SIGUSR1, multiplex_handler);
#else
	signal(SIGUSR1, multiplex_handler);
#endif
}

void multiplexer_destroy()
{
	close(dp);
}

void multiplexer_add(int fd, mevent_t event)
{
	struct pollfd pfd;

	pfd.fd = fd;
	pfd.events = 0;
	pfd.revents = 0;

	switch(event) {
		case MPLX_RD:
			pfd.events = POLLIN;
			break;
		case MPLX_WR:
			pfd.events = POLLOUT;
			break;
	}
	
	/* Prevent the polling loop from reacquiring the lock */
	rcl_write_reserve_lock(mlock);

	/* Interrupt the polling function so we can alter structures */
	if(!pthread_equal(pthread_self(), mtid))
		pthread_kill(mtid, SIGUSR1);

	/* Block until the polling loop has released the lock */
	rcl_write_complete_lock(mlock);

	write(dp, &pfd, sizeof(struct pollfd));

	rcl_write_unlock(mlock);
}

void multiplexer_remove(int fd, mevent_t event)
{
	struct pollfd pfd;

	pfd.fd = fd;
	pfd.events = POLLREMOVE;
	pfd.revents = 0;

	/* Prevent the polling loop from reacquiring the lock */
	rcl_write_reserve_lock(mlock);

	/* Interrupt the polling function so we can alter structures */
	if(!pthread_equal(pthread_self(), mtid))
		pthread_kill(mtid, SIGUSR1);

	/* Block until the polling loop has released the lock */
	rcl_write_complete_lock(mlock);

	write(dp, &pfd, sizeof(struct pollfd));

	rcl_write_unlock(mlock);
}

int multiplexer_poll(int *fd, mevent_t *event)
{
	struct dvpoll dvp;
	struct pollfd pfd;

	dvp.dp_fds = &pfd;
	dvp.dp_nfds = 1;
	dvp.dp_timeout = -1;

	rcl_read_lock(mlock);
	if(ioctl(dp, DP_POLL, &dvp) < 1)  {
		rcl_read_unlock(mlock);
		return -1;
	}
	rcl_read_unlock(mlock);

	if(fd != NULL)
		*fd = pfd.fd;

	if(event == NULL)
		return 0;

	*event = -1;

	if(pfd.revents & POLLIN)
		*event = MPLX_RD;
	else if(pfd.revents & POLLOUT)
		*event = MPLX_WR;

	return 0;
}
