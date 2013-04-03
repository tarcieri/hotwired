#include <pthread.h>

#include <HThread.h>
#include <MQueue.h>
#include <helper_thread.h>
#include <xmalloc.h>

struct _HThread {
	pthread_t tid;
	MQueue q;
};

HThread hthread_create()
{
	HThread t;
	
	t = NEW(HThread);
	t->q = mqueue_create();
	
	if(pthread_create(&t->tid, NULL, helper_thread, t) != 0) {
		mqueue_destroy(t->q);
		xfree(t);

		return NULL;
	}
	
	pthread_detach(t->tid);
	
	return t;
}

void hthread_destroy(HThread t)
{
	mqueue_send(t->q, HT_KILL);
	mqueue_destroy(t->q);

	xfree(t);
}

MQueue hthread_queue(HThread t)
{
	return t->q;
}
