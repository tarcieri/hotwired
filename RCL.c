/* Resource Control Locks

   For a detailed explanation, see RCL.h.  This implementation sits on top of
   POSIX.1 threads and is implemented using mutexes and conditions.
 */

#include <pthread.h>
#include <xmalloc.h>
#include <RCL.h>

struct _RCL {
	pthread_mutex_t entry_lock;
	pthread_mutex_t rc_lock;
	pthread_cond_t rc_cond;

	unsigned int rc;
};

RCL rcl_create()
{
	RCL ret = NEW(RCL);

	pthread_mutex_init(&ret->entry_lock, NULL);
	pthread_mutex_init(&ret->rc_lock, NULL);
	pthread_cond_init(&ret->rc_cond, NULL);

	ret->rc = 0;

	return ret;
}

void rcl_destroy(RCL rcl)
{
	pthread_mutex_destroy(&rcl->entry_lock);
	pthread_mutex_destroy(&rcl->rc_lock);
	pthread_cond_destroy(&rcl->rc_cond);

	xfree(rcl);
}

void rcl_read_lock(RCL rcl)
{
	pthread_mutex_lock(&rcl->entry_lock);
	pthread_mutex_lock(&rcl->rc_lock);
	rcl->rc++;
	pthread_mutex_unlock(&rcl->rc_lock);
	pthread_mutex_unlock(&rcl->entry_lock);
}

void rcl_read_unlock(RCL rcl)
{
	pthread_mutex_lock(&rcl->rc_lock);
	if(--rcl->rc == 0) 
		pthread_cond_signal(&rcl->rc_cond);
	pthread_mutex_unlock(&rcl->rc_lock);
}

void rcl_write_reserve_lock(RCL rcl)
{
	pthread_mutex_lock(&rcl->entry_lock);
}

void rcl_write_complete_lock(RCL rcl)
{
	pthread_mutex_lock(&rcl->rc_lock);

	if(rcl->rc > 0)
		pthread_cond_wait(&rcl->rc_cond, &rcl->rc_lock);
}

void rcl_write_lock(RCL rcl)
{
	rcl_write_reserve_lock(rcl);
	rcl_write_complete_lock(rcl);
}

void rcl_write_unlock(RCL rcl)
{
	pthread_mutex_unlock(&rcl->rc_lock);
	pthread_mutex_unlock(&rcl->entry_lock);
}
