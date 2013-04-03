#ifndef ACCOUNTMANAGER_H
#define ACCOUNTMANAGER_H

#include <Account.h>
#include <Permissions.h>
#include <Transaction.h>

void am_init();
void am_destroy();
int am_add_account_with_permissions(char *username, char *realname, char *password, Permissions permissions);
int am_add_account(char *username, char *realname, char *password, char *permissions);
int am_del_account(char *username);
void am_lock();
void am_unlock();
void am_xlock();
void am_xunlock();
Account am_lookup_account(char *username);
void am_commit();
TransactionOut am_build_userlist(TransactionIn t);

#endif
