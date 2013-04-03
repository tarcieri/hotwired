#include <global.h>

#include <ConnectionManager.h>
#include <Transaction.h>
#include <hlid.h>
#include <transaction_factories.h>
#include <xmalloc.h>

TransactionOut txn_join_create(int uid)
{
	TransactionOut ret;
	int16_t icon, status;
	char *nickname;

	if(cm_getval(uid, CONN_ICON, &icon) < 0)
		icon = 0;

	if(cm_getval(uid, CONN_STATUS, &status) < 0)
		status = 0;

	cm_getval(uid, CONN_NICKNAME, &nickname);

	ret = transaction_create(HL_ADDUSER, 4);
	transaction_add_int16(ret, HL_SOCKETNO, uid);
	transaction_add_int16(ret, HL_ICON, icon);
	transaction_add_int16(ret, HL_STATUS, status);
	transaction_add_string(ret, HL_NICKNAME, nickname);

	xfree(nickname);

	return ret;
}

TransactionOut txn_part_create(int uid)
{
	TransactionOut ret;

	ret = transaction_create(HL_DELUSER, 1);
	transaction_add_int16(ret, HL_SOCKETNO, uid);

	return ret;
}
