#include <pthread.h>
#include <stdlib.h>

#include <Config.h>
#include <Stack.h>
#include <HThread.h>
#include <ThreadManager.h>
#include <log.h>
#include <output.h>
#include <xmalloc.h>

static Stack inactive_threads = NULL;
static int active_thread_count = 0;
static int max_active_threads;
static int max_inactive_threads;

static pthread_mutex_t tm_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t tm_cond = PTHREAD_COND_INITIALIZER;

int tm_init()
{
	int i;
	HThread t;

	pthread_mutex_lock(&tm_mutex);

	if(inactive_threads == NULL)
		inactive_threads = stack_create();

	if((max_inactive_threads = config_int_value("threads", "max_inactive_threads")) < 1) {
		log("*** threads::max_inactive_threads invalid or unspecified, defaulting to %d", DEFAULT_MAX_INACTIVE_THREADS);
		max_inactive_threads = DEFAULT_MAX_INACTIVE_THREADS;
	}

	if((max_active_threads = config_int_value("threads", "max_active_threads")) < 1) {
		log("*** threads::max_active_threads invalid or unspecified, defaulting to %d", DEFAULT_MAX_ACTIVE_THREADS);
		max_active_threads = DEFAULT_MAX_ACTIVE_THREADS;
	}

	for(i = stack_size(inactive_threads) + active_thread_count; i < max_inactive_threads; i++) {
		if((t = hthread_create()) != NULL) 
			stack_push(inactive_threads, t);
		else
			log("!!! Warning: Error spawning thread while populating thread pool");
	}

	pthread_mutex_unlock(&tm_mutex);

	return 0;
}

void tm_destroy()
{
	stack_destroy(inactive_threads);
}

HThread tm_get_thread()
{
	HThread ret;

	pthread_mutex_lock(&tm_mutex);

#ifdef DEBUG
	debug("Requesting thread (Active: %d, Inactive: %d)", active_thread_count, stack_size(inactive_threads));
#endif

	while(active_thread_count >= max_active_threads)
		pthread_cond_wait(&tm_cond, &tm_mutex);

	if((ret = stack_pop(inactive_threads)) == NULL && (ret = hthread_create()) == NULL) {
		log("!!! Warning: Soft thread count limit exceeds hard limit.  Decrease threads::max_active_threads");
		pthread_cond_wait(&tm_cond, &tm_mutex);
		if((ret = stack_pop(inactive_threads)) == NULL) 
			fatal("!!! Fatal error: Thread pool depleted - system may be low on resources");
	}

	active_thread_count++;

	pthread_mutex_unlock(&tm_mutex);

	return ret;
}

void tm_return_thread(HThread t)
{
	pthread_mutex_lock(&tm_mutex);

	if(stack_size(inactive_threads) >= max_inactive_threads)
		hthread_destroy(t);
	else 
		stack_push(inactive_threads, t);

	active_thread_count--;
	
#ifdef DEBUG
	debug("Thread returned (Active: %d, Inactive: %d)", active_thread_count, stack_size(inactive_threads));
#endif

	pthread_cond_signal(&tm_cond);
	pthread_mutex_unlock(&tm_mutex);
}

void tm_handle_crash()
{
	pthread_mutex_lock(&tm_mutex);

	active_thread_count--;
	
#ifdef DEBUG
	debug("Thread crashed (Active: %d, Inactive: %d)\n", active_thread_count, stack_size(inactive_threads));
#endif

	pthread_cond_signal(&tm_cond);
	pthread_mutex_unlock(&tm_mutex);
}
