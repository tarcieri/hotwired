#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <Permissions.h>

typedef struct _Account {
	char *username;
	char *realname;
	char *password;
	Permissions perms;
} *Account;

Account account_create(char *username, char *realname, char *password, Permissions permissions);
void account_destroy(Account a);

#endif
