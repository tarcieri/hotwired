#ifndef TRANSACTION_REPLIES_H
#define TRANSACTION_REPLIES_H

#include <Transaction.h>

void reply_success(int uid, TransactionIn t);
void reply_error(int uid, TransactionIn t, char *format, ...);
void message_error(int uid, char *format, ...);

#endif
