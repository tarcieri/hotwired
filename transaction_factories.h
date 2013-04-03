#ifndef TRANSACTION_FACTORIES_H
#define TRANSACTION_FACTORIES_H

#include <Transaction.h>

/* Transaction factories */
TransactionOut txn_join_create(int uid);
TransactionOut txn_part_create(int uid);

#endif
