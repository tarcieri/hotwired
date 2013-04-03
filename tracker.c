#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <Config.h>
#include <ConnectionManager.h>
#include <log.h>
#include <tracker.h>
#include <util.h>
#include <xmalloc.h>

#define DEFAULT_HOTLINE_PORT		5500
#define DEFAULT_TRACKER_PORT		5499
#define DEFAULT_UPDATE_INTERVAL		150

typedef struct _TListNode {
	int fd;
	struct sockaddr_in sin;

	struct _TListNode *l;
} *TListNode;

static char *server_name = NULL, *server_desc = NULL;
static int server_name_l, server_desc_l;
static int server_port = DEFAULT_HOTLINE_PORT;
static int update_interval = DEFAULT_UPDATE_INTERVAL;
static int tlist_count = 0;
static TListNode tlist = NULL;

static int tthread_active = 0;
static pthread_t ttid;
static pthread_mutex_t tlock = PTHREAD_MUTEX_INITIALIZER;

static void tlist_add(char *str)
{
	char *hostname;
	int port;
	
	struct hostent *host;
	TListNode n = NEW(TListNode);

	hostname = str;
	if((str = strchr(hostname, ':')) != NULL) {
		*str++ = '\0';
		port = atoi(str);
	} else
		port = DEFAULT_TRACKER_PORT;

	memset(&(n->sin), '\0', sizeof(struct sockaddr_in));

	if((n->sin.sin_addr.s_addr = inet_addr(hostname)) < 0) {
		if((host = xgethostbyname(hostname)) == NULL)
			goto err;

		memcpy(&n->sin.sin_addr, host->h_addr, host->h_length);
		xfree(host);
	}

	n->sin.sin_family = AF_INET;
	n->sin.sin_port = htons(port);

	if((n->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0)
		goto err;

	n->l = tlist;
	tlist = n;
	tlist_count++;

	return;
err:
	log("!!! Couldn't resolve tracker: %s", hostname);
	xfree(n);
}

static void tlist_clear()
{
	TListNode n;

	while(tlist != NULL) {
		n = tlist;
		tlist = tlist->l;

		close(n->fd);
		xfree(n);
	}

	tlist_count = 0;
}

static void tracker_broadcast()
{
	TListNode c;
	
	uint8_t *dgram;
	uint16_t users;

	dgram = (uint8_t *)xcalloc(1, 14 + server_name_l + server_desc_l);
	users = cm_user_count();

	/* Version code */
	*(uint16_t *)dgram = htons(1);

	/* Server port */
	*(uint16_t *)(dgram + 2) = htons(server_port);

	/* Number of users on server */
	*(uint16_t *)(dgram + 4) = htons(users);

	/* Server name */
	dgram[12] = server_name_l;
	memcpy(dgram + 13, server_name, server_name_l);

	/* Server description */
	dgram[13 + server_name_l] = server_desc_l;
	memcpy(dgram + 14 + server_name_l, server_desc, server_desc_l);

	/* Broadcast datagram to trackers */
	for(c = tlist; c != NULL; c = c->l)
		sendto(c->fd, dgram, 14 + server_name_l + server_desc_l, 0, (struct sockaddr *)&(c->sin), sizeof(struct sockaddr_in));

	xfree(dgram);
}

static void *tracker_main(void *ptr)
{
	int sleep_period;

	pthread_mutex_lock(&tlock);
	log("*** Tracker thread started.  Updating %d trackers every %d seconds", tlist_count, update_interval);
	
	while(tlist != NULL) {
		tracker_broadcast();
		sleep_period = update_interval;
		
		pthread_mutex_unlock(&tlock);
		sleep(sleep_period);
		pthread_mutex_lock(&tlock);
	}

	pthread_mutex_unlock(&tlock);
	return NULL;
}

static void tracker_thread_spawn()
{
	if(tthread_active)
		return;

	if(pthread_create(&ttid, NULL, tracker_main, NULL) < 0) {
		log("!!! Warning: Couldn't spawn tracker thread");
		return;
	}
	
	tthread_active = 1;
}

void tracker_init()
{
	int i, v;
	char *t;

	pthread_mutex_lock(&tlock);
	tlist_clear();
	
	for(i = 0; (t = config_vlist_value("trackers", "host_list", i)) != NULL; i++) {
		tlist_add(t);
		xfree(t);
	}

	if(i == 0) 
		goto done;
	
	t = config_value("global", "server_name");

	if(server_name == NULL && t == NULL) {
		log("*** Set global::server_name if you wish to broadcast to trackers");
		log("*** No broadcasting to trackers will be done with the current configuration");
		goto done;
	}

	if(t != NULL) {
		if(server_name != NULL)
			xfree(server_name);

		server_name = t;
		server_name_l = strlen(server_name);
	}

	t = config_value("global", "server_description");

	if(server_desc != NULL) {
		if(t != NULL) { 
			xfree(server_desc); 
			server_desc = NULL;
		}
	} else if(t == NULL)
		t = xstrdup("");

	if(server_name == NULL) {
		server_desc = t;
		server_desc_l = strlen(server_desc);
	}

	if((v = config_int_value("trackers", "update_interval")) < 0) {
		log("*** trackers::update_interval invalid/unspecified");
	} else
		update_interval = v;

	if((server_port = config_int_value("global", "port")) < 0)
		server_port = DEFAULT_HOTLINE_PORT;

	tracker_thread_spawn();
done:
	pthread_mutex_unlock(&tlock);
}
