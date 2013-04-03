#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include <Account.h>
#include <AccountManager.h>
#include <Config.h>
#include <HashTable.h>
#include <Permissions.h>
#include <RCL.h>
#include <hlid.h>
#include <log.h>
#include <output.h>
#include <util.h>
#include <xmalloc.h>

typedef struct _AccountNode {
	Account a;
	struct _AccountNode *l;
} *AccountNode;

static HashTable account_table = NULL;
static AccountNode am_head = NULL, am_tail = NULL;
static RCL am_rcl = NULL;

pthread_mutex_t hlpasswd_lock = PTHREAD_MUTEX_INITIALIZER;

static FILE *hlpasswd_open(char *mode)
{
	FILE *ret;
	int l;
	char *filename, *base_path, *hlpasswd_name;

	base_path = config_value("paths", "base_path");

	if((hlpasswd_name = config_value("paths", "account_file")) == NULL)
		hlpasswd_name = xstrdup("hlpasswd");

	if(base_path != NULL) {
		if((l = strlen(base_path)) > 1 && base_path[l - 1] == '/')
			base_path[l - 1] = '\0';

		filename = xasprintf("%s/%s", base_path, hlpasswd_name);

		xfree(base_path);
		xfree(hlpasswd_name);
	} else 
		filename = hlpasswd_name;

	if((ret = fopen(filename, mode)) == NULL) 
		log("!!! Couldn't open '%s': %s", filename, strerror(errno));

	xfree(filename);

	return ret;
}

static void process_hlpasswd(char *line)
{
	char *username, *realname, *password, *permissions;

	username = line;

	if(*username == ':')
		return;

	if((password = strchr(username, ':')) == NULL)
		return;

	*password++ = '\0';

	if((permissions = strchr(password, ':')) == NULL)
		return;

	*permissions++ = '\0';

	if((realname = strchr(permissions, ':')) == NULL)
		return;

	*realname++ = '\0';

	if(*password == '\0')
		password = NULL;

	if(*realname == '\0')
		realname = NULL;
	else if((line = strchr(realname, '\n')) != NULL)
		*line = '\0';

	am_add_account(username, realname, password, permissions);
}

static void am_destroy_structures() 
{ 
	AccountNode c = am_head, t; 

	hash_table_destroy(account_table, (void(*)(void *))account_destroy);

	while(c != NULL) { 
		t = c; 
		c = c->l; 
		xfree(t); 
	}
} 

void am_init()
{
	FILE *hlpasswd;
	char buffer[256];

	if(am_rcl == NULL)
		am_rcl = rcl_create();

	rcl_write_lock(am_rcl);
	
	if(account_table != NULL) 
		am_destroy_structures();

	account_table = hash_table_create();
	am_head = am_tail = NULL;

	rcl_write_unlock(am_rcl);

	pthread_mutex_lock(&hlpasswd_lock);
	if((hlpasswd = hlpasswd_open("r")) == NULL) 
		goto err;

	while(fgets(buffer, 256, hlpasswd) != NULL) 
		process_hlpasswd(buffer);

	fclose(hlpasswd);

	if(am_head == NULL) {
		log("!!! No valid accounts found in hlpasswd file");
		goto err;
	}

	pthread_mutex_unlock(&hlpasswd_lock);
	return;
err:
	pthread_mutex_unlock(&hlpasswd_lock);

	log("!!! Falling back to passwordless admin account, login and change!");

	am_add_account("admin", "Administrator", NULL, "FFF3CFFFFF800000");
}

void am_destroy()
{
	am_destroy_structures();
	rcl_destroy(am_rcl);
}

int am_add_account_with_permissions(char *username, char *realname, char *password, Permissions permissions)
{
	Account a;

	if(username == NULL) 
		return -1;

	rcl_write_lock(am_rcl);

	if(account_table == NULL)
		account_table = hash_table_create();

	a = account_create(username, realname, password, permissions);
	if(hash_table_insert(account_table, username, a) < 0) {
		account_destroy(a);
		rcl_write_unlock(am_rcl);
		return -1;
	}

	if(am_head == NULL)
		am_head = am_tail = NEW(AccountNode);
	else {
		am_tail->l = NEW(AccountNode);
		am_tail = am_tail->l;
	}

	am_tail->a = a;
	am_tail->l = NULL;

	rcl_write_unlock(am_rcl);

	return 0;
}

int am_add_account(char *username, char *realname, char *password, char *permissions)
{
	int ret;
	Permissions p = permissions_create_from_ascii(permissions);

	if(!p) p = permissions_create_from_ascii("0000000000000000");
	
	ret = am_add_account_with_permissions(username, realname, password, p);
	permissions_destroy(p);
}

int am_del_account(char *username)
{
	Account a;
	AccountNode n, t;

	rcl_write_lock(am_rcl);

	if((a = (Account)hash_table_remove(account_table, username)) == NULL) {
		rcl_write_unlock(am_rcl);
		return -1;
	}

	if(am_head->a == a) {
		t = am_head;
		am_head = t->l;
		
		if(am_head == NULL)
			am_tail = NULL;
	} else {
		for(n = am_head; n->l != NULL && n->l->a != a; n = n->l);

		if(n->l == NULL) {
#ifdef DEBUG
			debug("INTERNAL INCONSISTENCY in am_del_account(): Account is not in account list!");
#endif
			rcl_write_unlock(am_rcl);
			return 0;
		}

		if(n->l == am_tail)
			am_tail = n;
		
		t = n->l;
		n->l = n->l->l;
	}

	xfree(t);
	account_destroy(a);

	rcl_write_unlock(am_rcl);

	return 0;
}

void am_lock()
{
	rcl_read_lock(am_rcl);
}

void am_unlock()
{
	rcl_read_unlock(am_rcl);
}

void am_xlock()
{
	rcl_write_lock(am_rcl);
}

void am_xunlock()
{
	rcl_write_unlock(am_rcl);
}

/* Calls to this must be preceeded by an am_rcl statement, then am_unlocked 
   when use of the account structure is finished */
Account am_lookup_account(char *username)
{
	return (Account)hash_table_lookup(account_table, username);
}

/* Write an account to the hlpasswd file */
static void write_account(FILE *hlpasswd, Account a)
{
	char *s, *password;

	if(!(password = a->password))
		password = "";

	fprintf(hlpasswd, "%s:%s:%s:%s\n", a->username, password, s = permissions_to_ascii(a->perms), a->realname);
	xfree(s);
}

/* Write the hlpasswd file */
void am_commit()
{
	FILE *hlpasswd;
	AccountNode n;

	pthread_mutex_lock(&hlpasswd_lock);
	if((hlpasswd = hlpasswd_open("w")) == NULL) {
		log("!!! Error: Couldn't save account information");
		pthread_mutex_unlock(&hlpasswd_lock);
		return;
	}

	rcl_read_lock(am_rcl);
	for(n = am_head; n != NULL; n = n->l) 
		write_account(hlpasswd, n->a);
	rcl_read_unlock(am_rcl);

	fclose(hlpasswd);
	pthread_mutex_unlock(&hlpasswd_lock);
}

void add_userlist_entry(TransactionOut t, Account a)
{
	uint8_t *buf;
	int i, username_len, realname_len, len;

	username_len = strlen(a->username);
	realname_len = strlen(a->realname);

	len = 27 + username_len + realname_len;
	buf = (uint8_t *)xmalloc(len);
	
	mcpy_int16(buf, 4);
	
	mcpy_int16(buf + 2, HL_NICKNAME);
	mcpy_int16(buf + 4, realname_len);
	memcpy(buf + 6, a->realname, realname_len);
	
	mcpy_int16(buf + 6 + realname_len, HL_USERNAME);
	mcpy_int16(buf + 8 + realname_len, username_len);
	for(i = 0; i < username_len; i++)
		buf[10 + realname_len + i] = a->username[i] ^ 0xFF;

	mcpy_int16(buf + 10 + realname_len + username_len, HL_PASSWORD);
	mcpy_int16(buf + 12 + realname_len + username_len, 1);
	buf[14 + realname_len + username_len] = 'x';

	mcpy_int16(buf + 15 + realname_len + username_len, HL_PERMISSION);
	mcpy_int16(buf + 17 + realname_len + username_len, 8);
	memcpy(buf + 19 + realname_len + username_len, a->perms->permissions, 8);

	transaction_add_object(t, HL_MESSAGE, buf, len);
}

TransactionOut am_build_userlist(TransactionIn t)
{
	TransactionOut ret;
	AccountNode n;

	rcl_read_lock(am_rcl);
	ret = transaction_reply_create(t, 0, hash_table_size(account_table));
	
	for(n = am_head; n != NULL; n = n->l)
		add_userlist_entry(ret, n->a);

	rcl_read_unlock(am_rcl);

	return ret;
}
