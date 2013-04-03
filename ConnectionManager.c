#include <global.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <Collection.h>
#include <ConnectionManager.h>
#include <Permissions.h>
#include <RCL.h>
#include <hlid.h>
#include <output.h>
#include <xmalloc.h>

typedef struct _Connection {
	/* 32-bit IPv4 IP address, in network byte order */
	uint32_t ip;

	/* Connection state */
	cstate_t cstate;

	/* Current server task number */
	uint32_t taskno;

	/* User icon */
	uint16_t icon;

	/* User status bits */
	uint16_t status;

	/* Time of last activity*/
	time_t last_activity;

	/* Login name */
	char *login;

	/* Nickname */
	char *nickname;

	/* Permissions */
	Permissions perms;

	/* Connection lock */
	RCL lock;
} *Connection;

static RCL cm_lock = NULL;
static Collection ctbl = NULL;

void cm_init(void)
{
	if(cm_lock == NULL)
		cm_lock = rcl_create();

	if(ctbl == NULL)
		ctbl = collection_create();
}

void cm_shutdown(void)
{
	rcl_write_lock(cm_lock);

	/* XXX Free the connection table here */
	collection_destroy(ctbl, NULL);

	rcl_destroy(cm_lock);
}

void cm_add(int fd, uint32_t ip)
{
	Connection c;

	rcl_write_lock(cm_lock);

	c = NEW(Connection);
	c->ip = ip;
	c->cstate = CSTATE_NL;
	c->taskno = 0;
	c->icon = 0;
	c->status = 0;
	c->last_activity = time(NULL);
	c->login = NULL;
	c->nickname = xstrdup("");
	c->perms = NULL;
	c->lock = rcl_create();

	collection_insert(ctbl, fd, c);

	rcl_write_unlock(cm_lock);
}

void cm_remove(int uid)
{
	Connection c;

	rcl_write_lock(cm_lock);

	if((c = collection_remove(ctbl, uid)) == NULL) {
		rcl_write_unlock(cm_lock);
		return;
	}

	rcl_write_unlock(cm_lock);
	rcl_write_lock(c->lock);

	if(c->login != NULL)
		xfree(c->login);

	xfree(c->nickname);

	if(c->perms != NULL)
		permissions_destroy(c->perms);

	rcl_destroy(c->lock);
	close(uid);
	xfree(c);
}

int cm_user_count(void)
{
	int ret;
	
	rcl_read_lock(cm_lock);
	ret = collection_size(ctbl);
	rcl_read_unlock(cm_lock);

	return ret;
}

int cm_getval(int uid, conn_member_t mid, void *ptr)
{
	Connection c;

	rcl_read_lock(cm_lock);

	if((c = collection_lookup(ctbl, uid)) == NULL) {
		rcl_read_unlock(cm_lock);
		return -1;
	}

	rcl_read_lock(c->lock);

	switch(mid) {
		case CONN_IP:
			memcpy(ptr, &c->ip, 4);
			break;
		case CONN_CSTATE:
			memcpy(ptr, &c->cstate, sizeof(cstate_t));
			break;
		case CONN_TASKNO:
			memcpy(ptr, &c->taskno, sizeof(cstate_t));
			break;
		case CONN_ICON:
			memcpy(ptr, &c->icon, 2);
			break;
		case CONN_STATUS:
			memcpy(ptr, &c->status, 2);
			break;
		case CONN_ACTIVITY:
			memcpy(ptr, &c->last_activity, sizeof(time_t));
			break;
		case CONN_LOGIN:
			*(char **)ptr = xstrdup(c->login);
			break;
		case CONN_NICKNAME:
			*(char **)ptr = xstrdup(c->nickname);
			break;
	}

	rcl_read_unlock(c->lock);
	rcl_read_unlock(cm_lock);

	return 0;
}

int cm_setval(int uid, conn_member_t mid, void *ptr)
{
	Connection c;

	rcl_read_lock(cm_lock);

	if((c = collection_lookup(ctbl, uid)) == NULL) {
		rcl_read_unlock(cm_lock);
		return -1;
	}

	rcl_write_lock(c->lock);

	switch(mid) {
		case CONN_IP:
			memcpy(&c->ip, ptr, 4);
			break;
		case CONN_CSTATE:
			memcpy(&c->cstate, ptr, sizeof(cstate_t));
			break;
		case CONN_TASKNO:
			memcpy(&c->taskno, ptr, 4);
			break;
		case CONN_ICON:
			memcpy(&c->icon, ptr, 4);
			break;
		case CONN_STATUS:
			memcpy(&c->status, ptr, 2);
			break;
		case CONN_ACTIVITY:
			memcpy(&c->last_activity, ptr, sizeof(time_t));
			break;
		case CONN_LOGIN:
			if(c->login != NULL)
				xfree(c->login);

			c->login = xstrdup((char *)ptr);
			break;
		case CONN_NICKNAME:
			xfree(c->nickname);
			c->nickname = xstrdup((char *)ptr);
			break;
	}

	rcl_write_unlock(c->lock);
	rcl_read_unlock(cm_lock);

	return 0;
}

cstate_t cm_get_cstate(int uid)
{
	cstate_t ret;

	if(cm_getval(uid, CONN_CSTATE, &ret) < 0)
		return CSTATE_NL;

	return ret;
}

int cm_set_cstate(int uid, cstate_t s)
{
	return cm_setval(uid, CONN_CSTATE, &s);
}

int32_t cm_get_taskno(int uid)
{
	int32_t ret;
	Connection c;

	rcl_read_lock(cm_lock);

	if((c = collection_lookup(ctbl, uid)) == NULL) {
		rcl_read_unlock(cm_lock);
		return -1;
	}

	rcl_write_lock(c->lock);
	ret = c->taskno++;
	rcl_write_unlock(c->lock);
	rcl_read_unlock(cm_lock);

	return ret;
}

Permissions cm_perm_getall(int uid)
{
	Connection c;
	Permissions ret = NULL;

	rcl_read_lock(cm_lock);

	if((c = collection_lookup(ctbl, uid)) == NULL) {
		rcl_read_unlock(cm_lock);
		return NULL;
	}

	rcl_read_lock(c->lock);
	if(c->perms != NULL)
		ret = permissions_copy(c->perms);
	rcl_read_unlock(c->lock);
	rcl_read_unlock(cm_lock);

	return ret;
}

int cm_perm_setall(int uid, Permissions p)
{
	Connection c;

	rcl_read_lock(cm_lock);

	if((c = collection_lookup(ctbl, uid)) == NULL) {
		rcl_read_unlock(cm_lock);
		return -1;
	}

	rcl_write_lock(c->lock);
	if(c->perms != NULL)
		permissions_destroy(c->perms);

	c->perms = permissions_copy(p);
	rcl_write_unlock(c->lock);
	rcl_read_unlock(cm_lock);

	return 0;
}

int cm_perm_check(int uid, int field)
{
	int ret = -1;
	Connection c;

	rcl_read_lock(cm_lock);

	if((c = collection_lookup(ctbl, uid)) == NULL) {
		rcl_read_unlock(cm_lock);
		return -1;
	}

	rcl_read_lock(c->lock);

	if(c->perms != NULL)  
		ret = permissions_check_field(c->perms, field);

	rcl_read_unlock(c->lock);
	rcl_read_unlock(cm_lock);

	return ret;
}

int cm_perm_set(int uid, int field, int value)
{
	int ret = 0;
	Connection c;

	rcl_read_lock(cm_lock);

	if((c = collection_lookup(ctbl, uid)) == NULL) {
		rcl_read_unlock(cm_lock);
		return -1;
	}

	rcl_write_lock(c->lock);

	if(c->perms == NULL)
		ret = -1;
	else if(value)
		permissions_set_field(c->perms, field);
	else
		permissions_unset_field(c->perms, field);

	rcl_write_unlock(c->lock);
	rcl_read_unlock(cm_lock);

	return ret;
}

struct cm_iterator_data {
	int (*iterator)(int uid, void *ptr);
	void *ptr;
};

static int cm_iterator(CollectionKey key, void *data, void *ptr)
{
	struct cm_iterator_data *id = (struct cm_iterator_data *)ptr;

	return id->iterator(key, id->ptr);
}

void cm_iterate(int (*iterator)(int uid, void *ptr), void *ptr)
{
	struct cm_iterator_data id;

	id.iterator = iterator;
	id.ptr = ptr;

	rcl_read_lock(cm_lock);
	collection_iterate(ctbl, cm_iterator, &id);
	rcl_read_unlock(cm_lock);
}

struct broadcast_data {
	int uid;
	TransactionOut t;
};

static int broadcast_iterator(CollectionKey uid, void *data, void *ptr)
{
	struct broadcast_data *d = (struct broadcast_data *)ptr;

	if(uid == d->uid)
		return 1;

	if(((Connection)data)->cstate == CSTATE_NL)
		return 1;

	transaction_write(uid, d->t);

	return 1;
}

void cm_transaction_broadcast(int uid, TransactionOut t)
{
	struct broadcast_data d;

	d.uid = uid;
	d.t = t;
	
	rcl_read_lock(cm_lock);
	collection_iterate(ctbl, broadcast_iterator, &d);
	rcl_read_unlock(cm_lock);

	transaction_out_destroy(t);
}

static int cm_userlist_iterator(CollectionKey uid, void *data, void *ptr)
{
	Connection c = (Connection)data;
	TransactionOut t = (TransactionOut)ptr;
	
	uint8_t *ulentry;
	int16_t v, l;

	rcl_read_lock(c->lock);
	if(c->cstate != CSTATE_LI)
		goto done;
		
	l = strlen(c->nickname);
	ulentry = xmalloc(l + 8);

	v = htons(uid);
	memcpy(ulentry, &v, 2);

	v = htons(c->icon);
	memcpy(ulentry + 2, &v, 2);

	v = htons(c->status);
	memcpy(ulentry + 4, &v, 2);

	v = htons(l);
	memcpy(ulentry + 6, &v, 2);

	memcpy(ulentry + 8, c->nickname, l);

	transaction_add_object(t, HL_USERLIST_ENTRY, ulentry, l + 8);
	xfree(ulentry);
done:
	rcl_read_unlock(c->lock);

	return 1;
}

TransactionOut cm_build_userlist(TransactionIn t)
{
	TransactionOut reply;
	
	rcl_read_lock(cm_lock);
	reply = transaction_reply_create(t, 0, collection_size(ctbl));
	collection_iterate(ctbl, cm_userlist_iterator, reply);
	rcl_read_unlock(cm_lock);

	return reply;
}
