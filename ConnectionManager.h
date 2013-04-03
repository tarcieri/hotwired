#ifndef CONNECTIONMANAGER_H
#define CONNECTIONMANAGER_H

#include <global.h>
#include <Permissions.h>
#include <Transaction.h>

typedef enum {
	CSTATE_NL,      /* Not logged in */
	CSTATE_NR,      /* Logged in, but haven't registered */
	CSTATE_LI       /* Logged in and registered */
} cstate_t;

typedef enum {
	CONN_IP,	/* uint32_t */
	CONN_CSTATE,	/* cstate_t */
	CONN_TASKNO,	/* uint32_t */
	CONN_ICON,	/* uint16_t */
	CONN_STATUS,	/* uint8_t */
	CONN_ACTIVITY,	/* time_t */
	CONN_LOGIN,	/* string */
	CONN_NICKNAME,	/* string */
} conn_member_t;

void cm_init(void);
void cm_shutdown(void);

void cm_add(int fd, uint32_t ip);
void cm_remove(int uid);

int cm_user_count(void);

int cm_getval(int uid, conn_member_t mid, void *ptr);
int cm_setval(int uid, conn_member_t mid, void *ptr);

cstate_t cm_get_cstate(int uid);
int cm_set_cstate(int uid, cstate_t s);
	
int32_t cm_get_taskno(int uid);

Permissions cm_perm_getall(int uid);
int cm_perm_setall(int uid, Permissions p);

int cm_perm_check(int uid, int field);
int cm_perm_set(int uid, int field, int value);

void cm_iterate(int (*iterator)(int uid, void *ptr), void *ptr);
void cm_transaction_broadcast(int uid, TransactionOut t);

TransactionOut cm_build_userlist(TransactionIn t);
	
#endif
