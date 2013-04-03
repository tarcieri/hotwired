#include <global.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <ConnectionManager.h>
#include <HThread.h>
#include <MQueue.h>
#include <Multiplexer.h>
#include <ThreadManager.h>
#include <TransferManager.h>
#include <Transaction.h>
#include <log.h>
#include <helper_thread.h>
#include <output.h>
#include <socketops.h>
#include <transaction_factories.h>
#include <transaction_handler.h>
#include <transfer_handler.h>
#include <util.h>
#include <xmalloc.h>

static pthread_key_t thread_key;
static pthread_once_t thread_once = PTHREAD_ONCE_INIT;

static void thread_once_init(void)
{
	pthread_key_create(&thread_key, NULL);
}

static void ht_control_connect(int fd)
{
	char hello[8], *s;
	uint16_t version;

	if(read_all(fd, hello, 8) < 0)
		goto err;

	if(strncmp("TRTPHOTL", hello, 8)) 
		goto err;

	if(read_int16(fd, &version) < 0) 
		goto err;

	if(read_int16(fd, &version) < 0) 
		goto err;

	if(write_all(fd, "TRTP\0\0\0\0", 8) < 0)
		goto err;

	multiplexer_add(fd, MPLX_RD);

	return;
err:
	log("--- Disconnect: %s (%d) failed handshake", s = conn_ntoa(fd), fd);
	xfree(s);
	cm_remove(fd);

	close(fd);
}

static void ht_data_connect(ConnectionInfo c)
{
	HThread t;
	char *s;
	int fd;
	uint32_t ip, tid;
#ifdef DEBUG
	uint32_t v1, v2;
#endif
	uint8_t buffer[16];

	fd = c->fd;
	ip = c->ip;
	xfree(c);

	if(read_all(fd, buffer, 16) < 0) 
		goto err;

	if(memcmp(buffer, "HTXF", 4)) 
		goto err;

	/* These are hopefully aligned properly... */
	tid = ntohl(*(uint32_t *)(buffer + 4));
#ifdef DEBUG
	/* The purpose of these two values is unknown.  They're always zero */
	v1 = ntohl(*(uint32_t *)(buffer + 8));
	v2 = ntohl(*(uint32_t *)(buffer + 12));

	debug("Got transfer: ID %d, v1: %d, v2: %d", tid, v1, v2);
#endif
	t = (HThread)pthread_getspecific(thread_key);
	
	if(tfm_register(tid, fd, ip, t) < 0)
		goto err;
		
	xfer_wait(fd, tid);

	return;
err:
	log("--- Disconnect: %s (%d) failed handshake", s = xinet_ntoa(ip), fd);
	close(fd);
}

static void ht_control_disconnect(int fd)
{
	tfm_purge_uid(fd);
	cm_transaction_broadcast(fd, txn_part_create(fd));
	cm_remove(fd);
}

static void ht_transaction(int fd)
{
	TransactionIn t;
	char *s;

#ifdef DEBUG
	debug("*** Helper thread handling transaction");
#endif

	if((t = transaction_read(fd)) == NULL)
		goto err0;

	if(th_exec(fd, t) < 0)
		goto err1;

	transaction_in_destroy(t);
	multiplexer_add(fd, MPLX_RD);

	return;

err1:
	transaction_in_destroy(t);
err0:
	log("--- Disconnect: %s (%d)", s = conn_ntoa(fd), fd);
	xfree(s);

	ht_control_disconnect(fd);
	return;
}

static int ht_exec(uint16_t id, MQBuf b)
{
	switch(id) {
		case HT_KILL:
			return 0;
		case HT_CONTROL_CONNECT:
			ht_control_connect(mqbuf_message_intval(b));
			break;
		case HT_DATA_CONNECT:
			ht_data_connect((ConnectionInfo)mqbuf_message_ptrval(b));
			break;
		case HT_TRANSACTION:
			ht_transaction(mqbuf_message_intval(b));
			break;
		case HT_UPLOAD:
		case HT_DOWNLOAD:
#ifdef DEBUG
			debug("!!! INTERNAL INCONSISTENCY: Stray HT_UPLOAD/DOWNLOAD caught by helper thread's event handler.  This is most likely a transfer manager bug");
#endif
			return 1;
	};

	tm_return_thread((HThread)pthread_getspecific(thread_key));
	return 1;
}

void *helper_thread(void *thread)
{
	HThread t = (HThread)thread;
	MQBuf b;

	pthread_once(&thread_once, thread_once_init);
	pthread_setspecific(thread_key, t);
	mqueue_init(hthread_queue(t));
	b = mqbuf_create();

#if DEBUG
	debug("*** New helper thread ready");
#endif

	do mqueue_recv(b);
	while(ht_exec(mqbuf_message_id(b), b));

	mqbuf_destroy(b);

	pthread_exit(NULL);
}
