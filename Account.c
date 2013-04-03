#include <string.h>

#include <Account.h>
#include <Permissions.h>
#include <global.h>
#include <output.h>
#include <xmalloc.h>

Account account_create(char *username, char *realname, char *password, Permissions permissions)
{
	Account a = NEW(Account);

	a->username = xstrdup(username);

	if(realname != NULL)
		a->realname = xstrdup(realname);
	else
		a->realname = NULL;

	if(password != NULL)
		a->password = xstrdup(password);
	else
		a->password = NULL;

	a->perms = permissions_copy(permissions);

	return a;
}

void account_destroy(Account a)
{
	xfree(a->username);

	if(a->realname != NULL)
		xfree(a->realname);

	if(a->password != NULL)
		xfree(a->password);

	if(a->perms != NULL)
		permissions_destroy(a->perms);

	xfree(a);
}
