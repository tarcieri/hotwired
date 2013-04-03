/* Abstract stateful multiplexer - implemented with poll() */

#include <sys/types.h>
#include <poll.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

#include <Multiplexer.h>
#include <RCL.h>
#include <output.h>
#include <machdep.h>
#include <xmalloc.h>

#define INITIAL_TABLE_SIZE	64

static RCL mlock = NULL;
static struct pollfd *fdtbl = NULL;
static unsigned int fdtbl_count, fdtbl_size, pcomplete, retfds, curfd;
static pthread_t mtid;

static void multiplex_handler(int i)
{
#ifdef DEBUG
	debug("*** Multiplex notifier activated");
#endif
}

void multiplexer_create()
{
	mtid = pthread_self();

	if(mlock == NULL)
		mlock = rcl_create();

	if(fdtbl == NULL) {
		fdtbl_count = 0;
		pcomplete = 0;
		fdtbl_size = INITIAL_TABLE_SIZE;
		fdtbl = (struct pollfd *)xmalloc(fdtbl_size * sizeof(struct pollfd));
	}

#ifdef HAVE_SIGSET
	sigset(SIGUSR1, multiplex_handler);
#else
	signal(SIGUSR1, multiplex_handler);
#endif
}

void multiplexer_destroy()
{
	xfree(fdtbl);
	fdtbl = NULL;
}

void multiplexer_add(int fd, mevent_t event)
{
	short pev = 0; 

	/* Prevent the polling loop from reacquiring the lock */
	rcl_write_reserve_lock(mlock);

	/* Interrupt the polling function so we can alter structures */
	if(!pthread_equal(pthread_self(), mtid))
		pthread_kill(mtid, SIGUSR1);

	/* Block until the polling loop has released the lock */
	rcl_write_complete_lock(mlock);

	/* If the table is full, double its size */
	if(fdtbl_count == fdtbl_size) {
		fdtbl_size *= 2;
		fdtbl = (struct pollfd *)xrealloc(fdtbl, fdtbl_size * sizeof(struct pollfd));
	}

	fdtbl[fdtbl_count].fd = fd;

	switch(event) {
		case MPLX_RD:
			pev = POLLIN;
			break;
		case MPLX_WR:
			pev = POLLOUT;
			break;
	}

	fdtbl[fdtbl_count].events = pev;
	fdtbl[fdtbl_count].revents = 0;

	fdtbl_count++;

	rcl_write_unlock(mlock);
}

void multiplexer_remove(int fd, mevent_t event)
{
	int i;

	/* Prevent the polling loop from reacquiring the lock */
	rcl_write_reserve_lock(mlock);

	/* Interrupt the polling function so we can alter structures */
	if(!pthread_equal(pthread_self(), mtid))
		pthread_kill(mtid, SIGUSR1);

	/* Block until the polling loop has released the lock */
	rcl_write_complete_lock(mlock);


	for(i = 0; i < fdtbl_count && fdtbl[i].fd != fd; i++);

	if(i == fdtbl_count) {
		rcl_write_unlock(mlock);
		return;
	}

	if(pcomplete && fdtbl[i].revents != 0)
		retfds--;

	if(i != fdtbl_count - 1) 
		memmove(fdtbl + i, fdtbl + i + 1, (fdtbl_count - i - 1) * sizeof(struct pollfd));

	fdtbl_count--;

	if(pcomplete && curfd > i)
		curfd--;

	/* If table is over 4 times as large as its contents, half its size */
	if(fdtbl_size / 4 > fdtbl_count && fdtbl_size > INITIAL_TABLE_SIZE) {
		fdtbl_size /= 2;
		fdtbl = (struct pollfd *)xrealloc(fdtbl, fdtbl_size * sizeof(struct pollfd));
	}

	rcl_write_unlock(mlock);
}

int multiplexer_poll(int *fd, mevent_t *event)
{
	rcl_read_lock(mlock);

	if(!pcomplete) {
		if((retfds = poll(fdtbl, fdtbl_count, -1)) < 0) {
			rcl_read_unlock(mlock);
			return -1;
		}

		pcomplete = 1;
		curfd = 0;
	}

	while(curfd < fdtbl_count && retfds > 0) {
		if(fdtbl[curfd].revents == 0) {
			curfd++;
			continue;
		}

		if(fd != NULL)
			*fd = fdtbl[curfd].fd;

		if(event == NULL) {
			curfd++;
			if(--retfds == 0)
				pcomplete = 0;

			rcl_read_unlock(mlock);
			return 0;
		}

		*event = -1;
		if(fdtbl[curfd].revents & POLLIN)
			*event = MPLX_RD;
		else if(fdtbl[curfd].revents & POLLOUT)
			*event = MPLX_WR;

		curfd++;
		if(--retfds == 0)
			pcomplete = 0;

		rcl_read_unlock(mlock);
		return 0;
	}

	pcomplete = 0;
	rcl_read_unlock(mlock);

	return -1;
}
