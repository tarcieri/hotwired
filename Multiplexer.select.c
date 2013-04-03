/* Abstract stateful multiplexer - implemented with select() */

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

#include <Multiplexer.h>
#include <RCL.h>
#include <machdep.h>
#include <output.h>
#include <xmalloc.h>

typedef struct _DescNode {
	int fd;
	mevent_t event;

	struct _DescNode *l;
} *DescNode;

typedef struct _Multiplexer {
	int maxfd;
	uint32_t pcomplete : 1, retfds : 31;

	DescNode head, cur, tail;
	RCL lock;

	fd_set rfds;
	fd_set wfds;
} *Multiplexer;

static Multiplexer m = NULL;
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

	if(m != NULL)
		multiplexer_destroy();

	m = NEW(Multiplexer);

	m->pcomplete = 0;
	m->maxfd = 0;

	m->head = m->cur = m->tail = NULL;

	m->lock = rcl_create();
#ifdef HAVE_SIGSET
	sigset(SIGUSR1, multiplex_handler);
#else
	signal(SIGUSR1, multiplex_handler);
#endif
}

void multiplexer_destroy()
{
	DescNode tmp;

	while(m->head != NULL) {
		tmp = m->head;
		m->head = tmp->l;

		xfree(tmp);
	}

	rcl_destroy(m->lock);
	xfree(m);
}

void multiplexer_add(int fd, mevent_t event)
{
	/* Prevent the polling loop from reacquiring the lock */
	rcl_write_reserve_lock(m->lock);

	/* Interrupt the polling function so we can alter structures */
	if(!pthread_equal(pthread_self(), mtid))
		pthread_kill(mtid, SIGUSR1);

	/* Block until the polling loop has released the lock */
	rcl_write_complete_lock(m->lock);

	if(m->head == NULL) 
		m->cur = m->head = m->tail = NEW(DescNode);
	else {
		m->tail->l = NEW(DescNode);
		m->tail = m->tail->l;
	}

	m->tail->fd = fd;
	m->tail->event = event;
	m->tail->l = NULL;

	if(fd > m->maxfd && m->maxfd != -1)
		m->maxfd = fd;

	rcl_write_unlock(m->lock);
}

void multiplexer_remove(int fd, mevent_t event)
{
	DescNode c, t;

	/* Prevent the polling loop from reacquiring the lock */
	rcl_write_reserve_lock(m->lock);

	/* Interrupt the polling function so we can alter structures */
	if(!pthread_equal(pthread_self(), mtid))
		pthread_kill(mtid, SIGUSR1);

	/* Block until the polling loop has released the lock */
	rcl_write_complete_lock(m->lock);


	if(!pthread_equal(pthread_self(), mtid))
		pthread_kill(mtid, SIGUSR1);

	if(m->head == NULL)
		goto done;

	if(m->head->fd == fd) {
		t = m->head;
		m->head = m->head->l;

		xfree(t);
	} else {
		for(c = m->head; c->l != NULL && c->l->fd != fd; c = c->l);

		if(c->l == NULL) 
			goto done;

		if(c->l == m->cur)
			m->cur = m->cur->l;

		if(c->l == m->tail)
			m->tail = c;

		t = c->l;
		c->l = c->l->l;

		xfree(t);
	}

	if(fd == m->maxfd)
		m->maxfd = -1;

done:
	rcl_write_unlock(m->lock);
}

int multiplexer_poll(int *fd, mevent_t *event)
{
	int r, rfd, maxfd;
	int tfd;
	mevent_t revent;

	rcl_read_lock(m->lock);

	if(m->pcomplete == 0) {
		if(m->maxfd == 0)
			return -1;

		FD_ZERO(&m->rfds);
		FD_ZERO(&m->wfds);

		if(m->maxfd == -1) {
			for(m->cur = m->head; m->cur != NULL; m->cur = m->cur->l) {
				tfd = m->cur->fd;

				switch(m->cur->event) {
					case MPLX_RD:
						FD_SET(tfd, &(m->rfds));
						break;
					case MPLX_WR:
						FD_SET(tfd, &(m->wfds));
						break;
				}

				if(tfd > m->maxfd)
					m->maxfd = tfd;
			}
		} else { 
			for(m->cur = m->head; m->cur != NULL; m->cur = m->cur->l) {
				tfd = m->cur->fd;

				switch(m->cur->event) {
					case MPLX_RD:
						FD_SET(tfd, &m->rfds);
						break;
					case MPLX_WR:
						FD_SET(tfd, &m->wfds);
						break;
				}
			}
		}

		maxfd = m->maxfd;
		if((r = select(maxfd + 1, &m->rfds, &m->wfds, NULL, NULL)) < 0) {
			rcl_read_unlock(m->lock);
			return -1;
		}

		m->retfds = r;
		m->pcomplete = 1;

		m->cur = m->head;
	}

	while(m->cur != NULL && m->retfds > 0) {

		rfd = m->cur->fd;

		revent = -1;
		
		if(FD_ISSET(rfd, &m->rfds))
			revent = MPLX_RD;
		else if(FD_ISSET(rfd, &m->wfds))
			revent = MPLX_WR;

		m->cur = m->cur->l;

		if(revent != -1) {
			if(--m->retfds < 1)
				m->pcomplete = 0;

			if(fd != NULL)
				*fd = rfd;

			if(event != NULL)
				*event = revent;

			rcl_read_unlock(m->lock);
			return 0;
		}
	}

	m->pcomplete = 0;

	rcl_read_unlock(m->lock);
	return -1;
}
