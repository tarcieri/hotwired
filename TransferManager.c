#include <global.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#include <Collection.h>
#include <Config.h>
#include <ConnectionManager.h>
#include <HThread.h>
#include <IDM.h>
#include <Transaction.h>
#include <TransferManager.h>
#include <hlid.h>
#include <output.h>
#include <transfer_handler.h>
#include <xmalloc.h>

/* 
   Theory of operation:

   Hotline operates using a two socket model somewhat similar to passive FTP 
   (except in the case of Hotline, fixed port numbers are used)  That is to
   say the download is coordinated using a control port then the actual transfer
   takes place on a separate data port.  Hotline also allows for transfers to 
   be queued if a set number of downloads have been reached.

   Typically I've observed that clients will connect to the data port after
   they have been assigned a download slot.  That is to say, while in the queue
   they do not connect to the data port.  This makes for a small problem in
   that a hostile person or group of hostile people could disrupt the operation
   of the server by waiting to be assigned a download slot then never connecting
   on the data port.

   My solution is to give a fixed amount of time (see the DATA_CONNECT_TIMEOUT
   definition below for specific time) for a connection on the data port to
   be abritrated.

   In order to facilitate this timeout, a monitor thread is spawned.  This
   thread iterates a "sequester list".  Once the server notifies a client
   that its download slot is ready, its transfer is placed into this list.
   The monitor thread then waits for a transfer thread to signal it that
   a sequestered transfer has begun.  If the alloted period of time goes
   by without the thread being signaled, then it will drop the download
   entirely and allow another transfer to take its spot.

TESTING:

This component has not been extensively unit tested.  The following are cases
which may arise during this component's operation and the expected behaviour.
Any inconsistancies in this behaviour should be reported as bugs.

State - Download queue is empty, active transfers <= max transfers
Action - A new download is requested
Behaviour - Download is added directly to sequester list
Testing - Empirical testing shows this to be implemented properly

State - Download queue is empty, all download slots full
Action - A new download is requested
Behaviour - Download is added to queue
Testing - Empirical testing shows this to be implemented properly

State - Transfer is first in sequester list, monitor thread waiting
Action - Transfer is registered before timeout elapses
Behaviour - Transfer should be remove from sequester list and started
Testing - Empirical testing shows this to be implemented properly

State - Transfer is first in sequester list, monitor thread waiting
Action - Registration timeout elapses without a data connection
Behaviour - Transfer should be purged, monitor thread iterates list
Testing - XXX No testing of this behaviour has been done

State - Transfer is in sequester list but not first
Action - Transfer is registered
Behaviour - Transfer should be removed from sequester list, download thread should be notified and transfer begun
Testing - XXX No testing of this behaviour has been done

State - Transfer is in download queue
Action - Transfer is registered
Behaviour - Transfer should be registered successfully
Testing - XXX No testing of this behaviour has been done

State - Transfer is first in queue and not registered
Action - An active download is completed or terminated
Behaviour - Transfer should be moved to sequester list
Testing - Empirical testing shows this to be implemented properly

State - Transfer is first in queue and registered
Action - An active download is completed or terminated
Behaviour - Download thread should be notified and transfer begun
Testing - XXX No testing of this behaviour has been done
*/

typedef enum {
	T_UPLOAD,
	T_DOWNLOAD
} ttype_t;

typedef struct _Transfer {
	int tid;
	int uid;
	ttype_t type;
	char *filename;

	int mode;
	int offset;
	int position;

	int fd;
	HThread thread;
	struct timespec ts;
} *Transfer;

typedef struct _QueueNode {
	Transfer t;
	struct _QueueNode *l;
} *QueueNode;

static pthread_mutex_t tfm_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t tfm_cond = PTHREAD_COND_INITIALIZER;

/* Structures */
static IDM tfm_idm = NULL;
static QueueNode q_head = NULL, q_tail = NULL;
static QueueNode seq_head = NULL, seq_tail = NULL;
static Collection ttbl = NULL;

/* State variables */
static int queue_size = 0;
static int active_downloads = 0, active_uploads = 0;

/* Limit variables */
static int queue_enable = 1;
static int max_downloads = -1;
static int max_uploads = -1;

/* Prototype for monitor thread function */
static void *monitor_thread(void *arg);

static void transfer_destroy(Transfer t)
{
	idm_return(&tfm_idm, t->tid);

	if(t->fd != -1)
		close(t->fd);

	if(t->filename != NULL)
		xfree(t->filename);

	xfree(t);
}

static void queue_position_notify(int uid, int tid, int position)
{
	TransactionOut t = transaction_create(HL_QUEUE_POSITION_INFO, 2);
	transaction_add_int32(t, HL_XFERID, tid);
	transaction_add_int16(t, HL_QUEUE_POSITION, position);

	transaction_write(uid, t);
	transaction_out_destroy(t);
}

static void activate_upload(Transfer t)
{
	TransferInfo d = NEW(TransferInfo);

	d->filename = xstrdup(t->filename);
	d->mode	= t->mode;
	d->offset = t->offset;

	mqueue_send_ptr(hthread_queue(t->thread), HT_UPLOAD, d);
}

static void activate_download(Transfer t)
{
	TransferInfo d = NEW(TransferInfo);

	d->filename = xstrdup(t->filename);
	d->mode = t->mode;
	d->offset = t->offset;

	mqueue_send_ptr(hthread_queue(t->thread), HT_DOWNLOAD, d);
}

static void initialize_ts(Transfer t)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	t->ts.tv_sec = tv.tv_sec + DATA_CONNECT_TIMEOUT;
	t->ts.tv_nsec = tv.tv_usec * 1000;
}

static int monitor_add_from_queue()
{
	pthread_t tid;
	QueueNode n;

	if((n = q_head) == NULL) 
		return -1;

	while(n != NULL) {
		n->t->position--;
		queue_position_notify(n->t->uid, n->t->tid, n->t->position);
		n = n->l;
	}

	n = q_head;

	if((q_head = q_head->l) == NULL)
		q_tail = NULL;

	queue_size--;

	if(n->t->thread != NULL) {
		activate_download(n->t);
		xfree(n);
		return 0;
	}

	if(seq_head != NULL) {
		seq_tail->l = n;
		seq_tail = seq_tail->l;
		seq_tail->l = NULL;
	} else {
		/* If sequester list is empty, start the monitor thread */
		if(pthread_create(&tid, NULL, monitor_thread, NULL))
			fatal("Couldn't spawn monitor thread!");

		seq_tail = seq_head = n;
		seq_tail->l = NULL;
	}

	initialize_ts(seq_tail->t);

	return 0;
}

static void *monitor_thread(void *arg)
{
	QueueNode n;

#ifdef DEBUG
	debug("+++ monitor_thread started");
#endif

	pthread_mutex_lock(&tfm_lock);
	while(seq_head != NULL) {
		if(pthread_cond_timedwait(&tfm_cond, &tfm_lock, &seq_head->t->ts)) {
			collection_remove(ttbl, seq_head->t->tid);

			if(seq_head->t->type == T_DOWNLOAD) {
				if(monitor_add_from_queue() < 0)
					active_downloads--;
			} else
				active_uploads--;

			transfer_destroy(seq_head->t);
		} else {
			if(seq_head->t->type == T_DOWNLOAD)
				activate_download(seq_head->t);
			else
				activate_upload(seq_head->t);
		}

		n = seq_head;
		seq_head = seq_head->l;

		xfree(n);
	}

	seq_tail = NULL;
	pthread_mutex_unlock(&tfm_lock);

#ifdef DEBUG
	debug("--- monitor thread exiting");
#endif

	pthread_exit(NULL);
}

static void monitor_add(Transfer t)
{
	pthread_t tid;

	if(seq_head == NULL) {
		if(pthread_create(&tid, NULL, monitor_thread, NULL))
			fatal("Couldn't spawn monitor thread!");

		seq_head = seq_tail = NEW(QueueNode);
	} else {
		seq_tail->l = NEW(QueueNode);
		seq_tail = seq_tail->l;
	}

	seq_tail->l = NULL;
	seq_tail->t = t;
	initialize_ts(t);
}

void tfm_init()
{
	int v;

	pthread_mutex_lock(&tfm_lock);

	if(tfm_idm == NULL)
		tfm_idm = idm_create(1, 4096);

	if(ttbl == NULL)
		ttbl = collection_create();

	/* Queueing defaults to on */
	if(config_truth_value("transfers", "queue_enable"))
		queue_enable = 1;
	else
		queue_enable = 0;

	if((v = config_int_value("transfers", "max_downloads")) >= 0)
		max_downloads = v;

	if((v = config_int_value("transfers", "max_uploads")) >= 0)
		max_uploads = v;

	while(active_downloads < max_downloads && !monitor_add_from_queue())
		active_downloads++;

	pthread_mutex_unlock(&tfm_lock);
}

int tfm_add_download(int uid, char *filename, int mode, int offset)
{
	QueueNode n;
	Transfer t;
	int ret = -1, tid, enqueue = 0;

	pthread_mutex_lock(&tfm_lock);
	if(max_downloads != -1 && active_downloads >= max_downloads) { 
		if(!queue_enable)
			goto done;

		enqueue = 1;
	} 

	if(idm_get(&tfm_idm, &tid) < 0)
		goto done;

	t = NEW(Transfer);

	t->tid = tid;
	t->uid = uid;
	t->type = T_DOWNLOAD;
	t->filename = xstrdup(filename);
	t->mode = mode;
	t->offset = offset;
	t->fd = -1;
	t->thread = NULL;

	if(!enqueue) 
		t->position = 0;
	else
		t->position = ++queue_size;

	collection_insert(ttbl, tid, t);

	if(!enqueue) {
		monitor_add(t);
		active_downloads++;
	} else {
		if(q_head == NULL)  
			n = q_head = q_tail = NEW(QueueNode);
		else { 
			q_tail->l = n = NEW(QueueNode);
			q_tail = n;
		}

		n->t = t;
		n->l = NULL;
	}

	ret = tid;
done:
	pthread_mutex_unlock(&tfm_lock);
	return ret;
}

struct upload_conflict_iterator_data {
	char *filename;
	int ret;
};

static int upload_conflict_iterator(CollectionKey k, void *data, void *ptr)
{
	Transfer t = (Transfer)data;
	struct upload_conflict_iterator_data *d = (struct upload_conflict_iterator_data *)ptr;
		
	if(t->type != T_UPLOAD)
		return 1;

	if(!strcasecmp(d->filename, t->filename)) {
		d->ret = 1;
		return 0;
	}

	return 1;
}
	
int tfm_add_upload(int uid, char *filename, int mode, int offset)
{
	Transfer t;
	int ret = -1, tid;
	struct upload_conflict_iterator_data d;

	d.filename = filename;
	d.ret = 0;

	pthread_mutex_lock(&tfm_lock);
	collection_iterate(ttbl, upload_conflict_iterator, &d);
	if(d.ret) {
		ret = -2;
		goto done;
	}
	
	if(max_uploads != -1 && active_uploads >= max_uploads)
		goto done;

	if(idm_get(&tfm_idm, &tid) < 0)
		goto done;

	t = NEW(Transfer);
	t->tid = tid;
	t->uid = uid;
	t->type = T_UPLOAD;
	t->filename = xstrdup(filename);
	t->mode = mode;
	t->offset = offset;
	t->fd = -1;
	t->thread = NULL;

	collection_insert(ttbl, tid, t);
	monitor_add(t);
	active_uploads++;

	ret = tid;
done:
	pthread_mutex_unlock(&tfm_lock);
	return ret;
}

int tfm_position(int tid)
{
	Transfer t;
	int ret = -1;

	pthread_mutex_lock(&tfm_lock);
	if((t = collection_lookup(ttbl, tid)) == NULL)
		goto done;

	ret = t->position;
done:
	pthread_mutex_unlock(&tfm_lock);
	return ret;
}

int tfm_owner(int tid)
{
	Transfer t;
	int ret = -1;

	pthread_mutex_lock(&tfm_lock);
	if((t = collection_lookup(ttbl, tid)) == NULL)
		goto done;

	ret = t->uid;
done:
	pthread_mutex_unlock(&tfm_lock);
	return ret;
}

int tfm_register(int tid, int fd, uint32_t ip, HThread thread)
{
	int ret = -1;
	Transfer t;
	uint32_t real_ip;
	QueueNode c, n;

	pthread_mutex_lock(&tfm_lock);
	if((t = collection_lookup(ttbl, tid)) == NULL) 
		goto done0;

	if(cm_getval(t->uid, CONN_IP, &real_ip) < 0)
		goto done0;

	if(ip != real_ip)
		goto done0;

	if(t->thread != NULL)
		goto done0;

	t->fd = fd;
	t->thread = thread;

	/* If download is queued we don't need to worry about anything */
	if(t->type == T_DOWNLOAD && t->position) 
		goto done1;

	/* Otherwise  we need to pull it out of the sequester list */
	if(seq_head == NULL) {
#ifdef DEBUG
		debug("!!! INTERNAL INCONSISTENCY in register_download(): Sequester list is empty!");
#endif
		goto done0;
	}

	if(seq_head->t != t) {
		for(c = seq_head; c->l != NULL && c->t != t; c = c->l);

		if(c->l == NULL) {
#ifdef DEBUG
			debug("!!! INTERNAL INCONSISTENCY in register_download(): Transfer missing from sequester list");
#endif
			goto done0;
		}

		n = c->l;
		c->l = n->l;

		if(c->l == NULL)
			seq_tail = c;

		if(t->type == T_DOWNLOAD)
			activate_download(t);
		else
			activate_upload(t);

		xfree(n);
	} else 
		/* The entry at the head of the sequester list is handled by 
		   the monitor thread.  We just signal the monitor thread */
		pthread_cond_signal(&tfm_cond);

done1:
	t->fd = fd;
	t->thread = thread;
	ret = 0;
done0:
	pthread_mutex_unlock(&tfm_lock);
	return ret;
}

void tfm_remove(int tid)
{
	Transfer t;
	QueueNode c, n;

#ifdef DEBUG
	debug("tfm_remove() called for TID: %d", tid);
#endif

	pthread_mutex_lock(&tfm_lock);
	if((t = collection_remove(ttbl, tid)) == NULL)
		goto done0;

	if(t->thread == NULL) {
		if(t->type == T_DOWNLOAD && t->position) {
			if(q_head == NULL) {
#ifdef DEBUG
				debug("!!! INTERNAL INCONSISTENCY in tfm_remove(): Transfer marked as queued but queue is empty!");
#endif

				goto done1;
			} else if(q_head->t == t) {
				n = q_head;
				q_head = q_head->l;
				if((c = q_head) == NULL)
					q_tail = NULL;
			} else {

				for(c = q_head; c->l != NULL && c->l->t != t; c = c->l);

				if(c->l == NULL) {
#ifdef DEBUG
					debug("!!! INTERNAL INCONSISTENCY in tfm_remove(): Transfer marked as queued but not found in queue list!");
#endif

					goto done1;
				}

				if(c->l == q_tail)
					q_tail = c;

				n = c->l;
				c->l = c->l->l;
			}

			xfree(n);
			queue_size--;

			while(c != NULL) {
				c->t->position--;
				queue_position_notify(c->t->uid, c->t->tid, c->t->position);
				n = n->l;
			}
		} else {
			if(seq_head == NULL) {
#ifdef DEBUG
				debug("!!! INTERNAL INCONSISTENCY in tfm_remove(): Sequester list is empty!");
#endif
				goto done1; 
			} else if(seq_head->t == t) 
				/* Let the monitor thread dispose of the transfer */	
				goto done0;
			else {
				for(c = seq_head; c->l != NULL && c->t != t; c = c->l);

				if(c->l == NULL) {
#ifdef DEBUG
					debug("!!! INTERNAL INCONSISTENCY in register_download(): Transfer missing from sequester list");
#endif
					goto done1;
				}

				n = c->l;
				c->l = n->l;
				if(c->l == NULL)
					seq_tail = c;

				xfree(n);

				if(t->type == T_DOWNLOAD) {
					if(monitor_add_from_queue() < 0)
						active_downloads--;
				} else
					active_uploads--;
			}
		}
	} else {
		if(t->type == T_DOWNLOAD) {
			if(monitor_add_from_queue() < 0)
				active_downloads--;
		} else
			active_uploads--;
	}
done1:
	transfer_destroy(t);
done0:
	pthread_mutex_unlock(&tfm_lock);
}

struct uid_iterator_list {
	int uid;
	QueueNode head, tail;
};

static int purge_uid_iterator(CollectionKey k, void *data, void *ptr)
{
	struct uid_iterator_list *l = (struct uid_iterator_list *)ptr;

	if(((Transfer)data)->uid == l->uid) {
		if(l->head != NULL) {
			l->tail->l = NEW(QueueNode);
			l->tail = l->tail->l;
		} else 
			l->head = l->tail = NEW(QueueNode);

		l->tail->l = NULL;
		l->tail->t = (Transfer)data;
	}

	return 1;
}

/* Purge all transactions belonging to a specified UID.  For now this is an
   O(n) operation where n is the number of current transfers */
void tfm_purge_uid(int uid)
{
	QueueNode c, n;
	Transfer t;
	struct uid_iterator_list l;

#ifdef DEBUG
	debug("Purging all transfers that belong to %d", uid);
#endif

	l.uid = uid;
	l.head = l.tail = NULL;

	pthread_mutex_lock(&tfm_lock);
	collection_iterate(ttbl, purge_uid_iterator, &l);

	c = l.head;

	while(c != NULL) {
		n = c;
		c = c->l;

		t = collection_remove(ttbl, n->t->uid);
		xfree(n);

		if(t == NULL) {
#ifdef DEBUG
			debug("!!! INTERNAL INCONSISTENCY: Bizzarre collection_remove() result in tfm_purge_uid");
#endif
			continue;
		}

#ifdef DEBUG
		debug("Purging transfer with TID: %d", t->tid);
#endif
		if(t->type == T_DOWNLOAD) {
			if(t->position == 0)
				active_downloads--;
		} else
			active_uploads--;

		transfer_destroy(t);
	}

#ifdef DEBUG
	debug("Finished purging transfers for %d", uid);
#endif

	while(active_downloads < max_downloads && !monitor_add_from_queue())
		active_downloads++;

	pthread_mutex_unlock(&tfm_lock);
}
