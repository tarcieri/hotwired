#include <global.h>

#include <pthread.h>
#include <string.h>

#include <MQueue.h>
#include <xmalloc.h>

struct _MQueue;

union mqueue_value {
	int v;
	void *p;
};

typedef struct _MQueueNode {
	struct _MQueue *sq;
	uint16_t i;
	struct _MQueueNode *l;

	union mqueue_value d;
} *MQueueNode;

struct _MQueue {
	pthread_mutex_t mutex;
	pthread_cond_t cond;

	MQueueNode headptr, tailptr;
};

struct _MQBuf {
	struct _MQueue *sq;
	uint16_t i;

	union mqueue_value d;
};

static pthread_key_t mqueue_key;
static pthread_once_t mqueue_once = PTHREAD_ONCE_INIT;

void mqueue_destroy(MQueue q)
{
	MQueueNode c, t;

	pthread_mutex_destroy(&q->mutex);
	pthread_cond_destroy(&q->cond);

	c = q->headptr;
	while(c != NULL) {
		t = c->l;
		xfree(c);
		c = t;
	}

	xfree(c);
}

static void mqueue_destructor(void *ptr)
{
	mqueue_destroy((MQueue)ptr);
}

static void mqueue_once_init(void)
{
	pthread_key_create(&mqueue_key, mqueue_destructor);
}

MQueue mqueue_create()
{
	MQueue q;

	q = NEW(MQueue);

	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->cond, NULL);

	q->headptr = NULL;
	q->tailptr = NULL;

	return q;
}

void mqueue_init(MQueue q)
{
	pthread_once(&mqueue_once, mqueue_once_init);

	if(pthread_getspecific(mqueue_key) != NULL)
		return;

	if(q == NULL)
		q = mqueue_create();

	pthread_setspecific(mqueue_key, q);
}

MQueue mqueue_self()
{
	return (MQueue)pthread_getspecific(mqueue_key);
}

MQBuf mqbuf_create()
{
	MQBuf b = NEW(MQBuf);

	b->sq = NULL;
	b->i = 0;
	b->d.p = NULL;

	return b;
}

void mqbuf_destroy(MQBuf b)
{
	xfree(b);
}

static void mqueue_push(MQueue q, uint16_t id, union mqueue_value *v)
{
	if(q->headptr == NULL) 
		q->headptr = q->tailptr = NEW(MQueueNode);
	else {
		q->tailptr->l = NEW(MQueueNode);
		q->tailptr = q->tailptr->l;
	}

	q->tailptr->i = id;
	memcpy(&q->tailptr->d,  v, sizeof(union mqueue_value));
	q->tailptr->l = NULL;
}

static void mqueue_pop(MQueue q, MQBuf b)
{
	MQueueNode c;

	if((c = q->headptr) == NULL) {
		b->i = 0;
		b->d.p = NULL;

		return;
	}

	q->headptr = q->headptr->l;

	b->i = c->i;
	memcpy(&b->d, &c->d, sizeof(union mqueue_value));

	xfree(c);
}

void mqueue_send_ptr(MQueue q, uint16_t id, void *data)
{
	union mqueue_value v;

	v.p = data;

	pthread_mutex_lock(&q->mutex);

	mqueue_push(q, id, &v);

	pthread_cond_signal(&q->cond);
	pthread_mutex_unlock(&q->mutex);
}

void mqueue_send_int(MQueue q, uint16_t id, int val)
{
	union mqueue_value v;

	v.v = val;

	pthread_mutex_lock(&q->mutex);

	mqueue_push(q, id, &v);

	pthread_cond_signal(&q->cond);
	pthread_mutex_unlock(&q->mutex);
}

void mqueue_send(MQueue q, uint16_t id)
{
	mqueue_send_ptr(q, id, NULL);
}

void mqueue_reply_ptr(MQBuf b, uint16_t id, void *data)
{
	if(b->sq == NULL)
		return;

	mqueue_send_ptr(b->sq, id, data);
}

void mqueue_reply_int(MQBuf b, uint16_t id, int val)
{
	if(b->sq == NULL)
		return;
	
	mqueue_send_int(b->sq, id, val);
}

void mqueue_reply(MQBuf b, uint16_t id)
{
	if(b->sq == NULL)
		return;
	
	mqueue_send(b->sq, id);
}

void mqueue_recv(MQBuf b)
{
	MQueue q = mqueue_self();

	pthread_mutex_lock(&q->mutex);
	if(q->headptr != NULL) {
		mqueue_pop(q, b);
		pthread_mutex_unlock(&q->mutex);

		return;
	}

	pthread_cond_wait(&q->cond, &q->mutex);
	mqueue_pop(q, b);
	pthread_mutex_unlock(&q->mutex);
}

uint16_t mqbuf_message_id(MQBuf b)
{
	return b->i;
}

void *mqbuf_message_ptrval(MQBuf b)
{
	return b->d.p;
}

int mqbuf_message_intval(MQBuf b)
{
	return b->d.v;
}
