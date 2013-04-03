#include <global.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include <Account.h>
#include <AccountManager.h>
#include <Collection.h>
#include <Config.h>
#include <ConnectionManager.h>
#include <Permissions.h>
#include <Transaction.h>
#include <TransferManager.h>
#include <fileops.h>
#include <hlid.h>
#include <log.h>
#include <output.h>
#include <password.h>
#include <transaction_factories.h>
#include <transaction_handler.h>
#include <transaction_replies.h>
#include <util.h>
#include <xmalloc.h>

struct th_list_element {
	int16_t tid;
	int (*handler)(int uid, TransactionIn);
};

#define HANDLED_TRANSACTIONS	29
struct th_list_element transaction_list[HANDLED_TRANSACTIONS] = {
	{ HL_LOGIN, 		txn_login },
	{ HL_INFO, 		txn_info },
	{ HL_UPDATEUSER,	txn_info },
	{ HL_USERLIST,		txn_userlist },
	{ HL_FILELIST, 		txn_filelist },
	{ HL_DOWNLOAD_FILE,	txn_download },
	{ HL_UPLOAD_FILE, 	txn_upload },
	{ HL_DOWNLOAD_FOLDER,	txn_download_folder },
	{ HL_UPLOAD_FOLDER,	txn_upload_folder },
	{ HL_CANCEL_TRANSFER,	txn_xfer_cancel },
	{ HL_CREATE_FOLDER,	txn_create_folder },
	{ HL_DELETE, 		txn_delete },
	{ HL_MOVE,		txn_move },
	{ HL_RENAME,		txn_rename },
	{ HL_FILE_INFO,		txn_file_info },
	{ HL_READ_ACCOUNT,	txn_read_account },
	{ HL_CREATE_ACCOUNT,	txn_create_account },
	{ HL_MODIFY_ACCOUNT,	txn_modify_account },
	{ HL_DELETE_ACCOUNT,	txn_delete_account },
	{ HL_ADMIN_ACCOUNTS,	txn_admin_accounts },
	{ HL_ADMIN_SUBMIT,	txn_admin_submit },
	{ HL_KICKUSER,		txn_kick_user },
	{ HL_USERINFO,		txn_user_info },
	{ HL_BROADCAST,		txn_broadcast },
	{ HL_SENDCHAT,		txn_send_chat },
	{ HL_SENDPM,		txn_send_privmsg },
	{ HL_REQUEST_CHAT,	txn_request_chat },
	{ HL_GET_NEWS_BUNDLE,	txn_get_news_bundle },
	{ HL_PING, 		txn_ping }
};

Collection transaction_table;

static char *file_path(char *path, char *file)
{
	char *t, *ret;
	struct stat st;

	if(!path)
		return f_addpath(file);

	t = f_addpath(path);
	if(stat(t, &st) < 0 || !S_ISDIR(st.st_mode)) {
		xfree(t);
		return NULL;
	}

	if(t[strlen(t) - 1] != '/') 
		ret = xasprintf("%s/%s", t, file);
	else
		ret = xasprintf("%s%s", t, file);

	xfree(t);

	return ret;
}

void th_init()
{
	int i;

	transaction_table = collection_create();
	for(i = 0; i < HANDLED_TRANSACTIONS; i++)
		collection_insert(transaction_table, (CollectionKey)transaction_list[i].tid, (void *)transaction_list[i].handler);
}

int th_exec(int uid, TransactionIn t)
{
	int (*handler)(int, TransactionIn);

#ifdef DEBUG
	transaction_print(t);
#endif

	if((handler = (int (*)(int, TransactionIn))collection_lookup(transaction_table, (CollectionKey)transaction_id(t))) == NULL) {
		reply_error(uid, t, "Error: Unsupported/unimplemented transaction: %d\n", transaction_id(t));
		return 0;
	}

	return handler(uid, t);
}

int txn_login(int uid, TransactionIn t)
{
	int i;
	char *username, *password, *s;
	TransactionOut reply;
	Account a;

	if(cm_get_cstate(uid) != CSTATE_NL) {
		reply_error(uid, t, "Error: Already logged in.");
		return 0;
	}

	if((i = transaction_object_index(t, HL_USERNAME)) >= 0) 
		username = transaction_object_masked_string(t, i);
	else {
		if((username = config_value("users", "guest_account")) == NULL) 
			username = xstrdup("guest");
	}

	if((i = transaction_object_index(t, HL_PASSWORD)) >= 0)
		password = transaction_object_masked_string(t, i);
	else
		password = NULL;

	am_lock();

	/* Check for valid username */
	if((a = am_lookup_account(username)) == NULL) {
		am_unlock();

		reply_error(uid, t, "Error: Invalid login.");

		log("!!! Failed login from %s (%d): Invalid username \"%s\".", s = conn_ntoa(uid), uid, username);
		xfree(s);

		xfree(username);
		if(password)
			xfree(password);

		return 0;
	}

	/* Check for valid password */
	if(a->password != NULL && *a->password != '\0' && 
			(password == NULL || !password_check(a->password, password))) {
		am_unlock();

		reply_error(uid, t, "Error: Invalid login.");

		log("!!! Failed login from %s (%d): Invalid password for \"%s\".", s = conn_ntoa(uid), uid, username);
		xfree(s);

		xfree(username);
		if(password)
			xfree(password);

		return 0;
	}

	/* Set username for the connection */
	cm_setval(uid, CONN_LOGIN, a->username);
	xfree(username);
	xfree(password);

	/* Set connection variables */
	cm_perm_setall(uid, a->perms);
	cm_set_cstate(uid, CSTATE_NR);

	/* Successful login */
	reply = transaction_reply_create(t, 0, 3);

	/* Send server name if it has been set */
	if((s = config_value("global", "server_name")) != NULL) {
		transaction_add_string(reply, HL_SERVER_STRING, s);
		xfree(s);
	}

	transaction_add_int32(reply, HL_VERSION, SERVER_VERSION);
	transaction_write(uid, reply);
	transaction_out_destroy(reply);

	/* Send agreement */
	reply = transaction_create(HL_AGREEMENT, 0);
	transaction_write(uid, reply);
	transaction_out_destroy(reply);

	/* Send user permissions */
	reply = transaction_create(HL_PERMISSIONS, 1);
	transaction_add_object(reply, HL_PERMISSION, a->perms->permissions, 8);
	transaction_write(uid, reply);
	transaction_out_destroy(reply);

	log("*** Successful '%s' login from %s (%d)", a->username, s = conn_ntoa(uid), uid);
	xfree(s);

	am_unlock();

	return 0;
}

int txn_info(int uid, TransactionIn t)
{
	int i;
	uint16_t status = 0, status_bits;
	uint16_t icon;
	cstate_t cstate;
	Permissions p;

	if((cstate = cm_get_cstate(uid)) == CSTATE_NL) {
		reply_error(uid, t, "Not logged in.");
		return 0;
	}

	if((i = transaction_object_index(t, HL_NICKNAME)) >= 0)
		cm_setval(uid, CONN_NICKNAME, transaction_object_string(t, i));

	if((i = transaction_object_index(t, HL_ICON)) >= 0) {
		icon = transaction_object_int16(t, i);
		cm_setval(uid, CONN_ICON, &icon);
	}

	if((i = transaction_object_index(t, HL_MSGOPTS)) >= 0) {
		status_bits = transaction_object_int16(t, i);
		if(status_bits & HL_DISALLOW_PRIVMSG)
			status |= HL_STATUS_NO_PRIVMSG;

		if(status_bits & HL_DISALLOW_CHAT)
			status |= HL_STATUS_NO_CHAT;
	}

	if(cstate == CSTATE_NR) 
		cm_set_cstate(uid, CSTATE_LI);

	if((p = cm_perm_getall(uid)) == NULL)
		return -1;

	if(permissions_is_superuser(p))
		status |= HL_STATUS_SUPERUSER;

	if(permissions_is_operator(p))
		status |= HL_STATUS_ADMIN;

	if(permissions_is_visitor(p))
		status |= HL_STATUS_NOPERMISSIONS;

	permissions_destroy(p);
	cm_setval(uid, CONN_STATUS, &status);

	reply_success(uid, t);
	cm_transaction_broadcast(-1, txn_join_create(uid));

	return 0;
}

int txn_userlist(int uid, TransactionIn t)
{
	TransactionOut reply;

	reply = cm_build_userlist(t);
	transaction_write(uid, reply);
	transaction_out_destroy(reply);

	return 0;
}

int txn_filelist(int uid, TransactionIn t)
{
	TransactionOut reply;
	int i;
	char *path = NULL;

	if((i = transaction_object_index(t, HL_FILEPATH)) >= 0)
		path = transaction_object_path(t, i);

	reply = f_list_create(t, path);

	if(path)
		xfree(path);

	transaction_write(uid, reply);
	transaction_out_destroy(reply);

	return 0;
}

int txn_download(int uid, TransactionIn t)
{
	TransactionOut reply;
	int tid, i, transfer_mode = 0, queue_position;
	char *path = NULL, *file = NULL, *fp;
	int32_t offset = 0;
	uint8_t *resume_buffer;
	struct stat st;

	if(cm_perm_check(uid, HL_PERM_DOWNLOAD_FILES) < 1) {
		reply_error(uid, t, "Error: You are not allowed to download files.");
		return 0;
	}

	for(i = 0; i < transaction_object_count(t); i++) {
		switch(transaction_object_id(t, i)) {
			case HL_FILENAME:
				if(file) xfree(file);
				file = transaction_object_string(t, i);
				break;
			case HL_FILEPATH:
				if(path) xfree(path);
				path = transaction_object_path(t, i);
				break;
			case HL_RESUME_INFO:
				if(transaction_object_len(t, i) < 50)
					continue;

				resume_buffer = transaction_object_data(t, i);
				memcpy(&offset, resume_buffer + 46, 4);
				offset = ntohl(offset);
				break;
			case HL_TRANSFER_MODE:
				if(transaction_object_int16(t, i) == 2)
					transfer_mode = 1;
				break;
		}
	}

#ifdef DEBUG
	if(path != NULL) 
		debug("File: %s/%s requested for download", path, file);
	else
		debug("File: %s requested for download", file);
#endif

	if(file == NULL) {
		if(path)
			xfree(path);

		reply_error(uid, t, "Error: No file to download specified.");
		return 0;
	}

	fp = path;
	path = file_path(fp, file);
	xfree(fp);

#ifdef DEBUG
	debug("Full path is: %s", path);
#endif

	if(path == NULL || stat(path, &st) < 0) {
		if(path)
			xfree(path);

		reply_error(uid, t, "Error: File not found/no access.");
		return 0;
	}

	if(offset > st.st_size) {
		xfree(path);

		reply_error(uid, t, "Error: Can't resume download due to file size mismatch.");
		return 0;
	}

	if((tid = tfm_add_download(uid, path, transfer_mode, offset)) < 0) {
		xfree(path);

		reply_error(uid, t, "Error: No download slots available.");
		return 0;
	}

	reply = transaction_reply_create(t, 0, 4);
	transaction_add_int16(reply, HL_INFOLEN, 20);
	transaction_add_int32(reply, HL_FILELEN, st.st_size - offset);
	transaction_add_int32(reply, HL_XFERID, tid);
	if((queue_position = tfm_position(tid)) > 0)
		transaction_add_int16(reply, HL_QUEUE_POSITION, queue_position);

	transaction_write(uid, reply);
	transaction_out_destroy(reply);
	xfree(path);

	return 0;
}

int txn_upload(int uid, TransactionIn t)
{
	TransactionOut reply;
	int i, tid;
	char *path = NULL, *file = NULL, *fp;
	int16_t mode = 0;
	int32_t offset = 0;
	uint8_t resume_header[58];
	struct stat st;

	if(cm_perm_check(uid, HL_PERM_UPLOAD_FILES) < 1) {
		reply_error(uid, t, "Error: You are not allowed to upload files.");
		return 0;
	}

	for(i = 0; i < transaction_object_count(t); i++) {
		switch(transaction_object_id(t, i)) {
			case HL_FILENAME:
				if(file) xfree(file);
				file = transaction_object_string(t, i);
				break;
			case HL_FILEPATH:
				if(path) xfree(path);
				path = transaction_object_path(t, i);
				break;
			case HL_TRANSFER_MODE:
				mode = transaction_object_int16(t, i);
		}
	}

	if(file == NULL) {
		if(path)
			xfree(path);

		reply_error(uid, t, "Error: No filename specified.");
		return 0;
	}

	/* XXX Do path authorization here */
	if(cm_perm_check(uid, HL_PERM_UPLOAD_ANYWHERE) < 1) {
		if(path)
			xfree(path);
		xfree(file);

		reply_error(uid, t, "Error: You are not allowed to upload to that location.");
		return 0;
	}

	if(path != NULL) {
		fp = f_addpath(path);
		xfree(path);

		if(stat(fp, &st) < 0 || !S_ISDIR(st.st_mode)) {
			xfree(file);
			xfree(fp);

			reply_error(uid, t, "Error: Invalid path specified.");
			return 0;
		}

		path = xasprintf("%s/%s", fp, file);
		xfree(fp);
	} else  
		path = f_addpath(file);

#ifdef DEBUG
	debug("Full upload path is: %s", path);
#endif

	if(!stat(path, &st)) {
		reply_error(uid, t, "Error: The file '%s' already exists.", file);
		xfree(file);

		return 0;
	}

	xfree(file);

	if(mode) {
		file = xasprintf("%s.hpf", path);
#ifdef DEBUG
		debug("Attempting to resume %s\n", file);
#endif

		if(stat(file, &st) < 0 || !S_ISREG(st.st_mode)) {
			xfree(file);
			xfree(path);

			reply_error(uid, t, "Error: The specified file cannot be resumed because it is not a valid partial file."); 
			return 0; 
		}

		offset = st.st_size;
		xfree(file);
	}

	if((tid = tfm_add_upload(uid, path, mode, offset)) < 0) {
		xfree(path);

		if(tid == -2)
			reply_error(uid, t, "Error: Another user is currently uploading the specified file.");
		else 
			reply_error(uid, t, "Error: No upload slots available.");
		return 0;
	}

	reply = transaction_reply_create(t, 0, 2);
	transaction_add_int32(reply, HL_XFERID, tid);

	/* Some rather gimpy code for constructing a resumption header */
	if(mode) {
		memset(resume_header, 0, 58);

		memcpy(resume_header, "RFLT", 4);
		resume_header[5] = 1;
		resume_header[41] = 2;
		memcpy(resume_header + 42, "DATA", 4);
		offset = htonl(offset);
		memcpy(resume_header + 46, &offset, 4);

		transaction_add_object(reply, HL_RESUME_INFO, resume_header, 58);
	}

	transaction_write(uid, reply);
	transaction_out_destroy(reply);
	xfree(path);

	return 0;
}

int txn_download_folder(int uid, TransactionIn t)
{
	reply_error(uid, t, "Error: This server does not support folder downloads.");
	return 0;
}

int txn_upload_folder(int uid, TransactionIn t)
{
	reply_error(uid, t, "Error: This server does not support folder uploads.");
	return 0;
}

int txn_xfer_cancel(int uid, TransactionIn t)
{
	int i;
	int32_t tid = -1;

	if((i = transaction_object_index(t, HL_XFERID)) < 0) {
		reply_error(uid, t, "Error: No transfer ID specified");
		return 0;
	}

	tid = transaction_object_int32(t, i);

	if(tfm_owner(tid) != uid) {
		reply_error(uid, t, "Error: Invalid transfer ID");
		return 0;
	}

	tfm_remove(tid);
	reply_success(uid, t);

	return 0;
}

int txn_create_folder(int uid, TransactionIn t)
{
	int i, umask;
	char *file = NULL, *path = NULL, *fp;
	struct stat st;

	if(cm_perm_check(uid, HL_PERM_CREATE_FOLDERS) < 1) {
		reply_error(uid, t, "Error: You are not allowed to create folders.");
		return 0;
	}

	for(i = 0; i < transaction_object_count(t); i++) {
		switch(transaction_object_id(t, i)) {
			case HL_FILENAME:
				if(file) xfree(file);
				file = transaction_object_string(t, i);
				break;
			case HL_FILEPATH:
				if(path) xfree(path);
				path = transaction_object_path(t, i);
				break;
		}
	}

	if(file == NULL) {
		if(path)
			xfree(path);

		reply_error(uid, t, "Error: No folder name specified.");
		return 0;
	}

	if(path != NULL) {
		fp = f_addpath(path);
		xfree(path);

		if(stat(fp, &st) < 0 || !S_ISDIR(st.st_mode)) {
			xfree(file);
			xfree(fp);

			reply_error(uid, t, "Error: Invalid path specified.");
			return 0;
		}

		path = xasprintf("%s/%s", fp, file);
		xfree(fp);
	} else
		path = f_addpath(file);

	xfree(file);

	if((umask = config_int_value("transfers", "file_umask")) < 0)
		umask = 0755;

	if(mkdir(path, umask) < 0) {
		xfree(path);
		reply_error(uid, t, "Error: Couldn't create the specified directory.");
		return 0;
	}

	xfree(path);
	reply_success(uid, t);

	return 0;
}

int txn_delete(int uid, TransactionIn t)
{
	int i;
	char *file = NULL, *path = NULL, *fp;
	struct stat st;

	if(cm_perm_check(uid, HL_PERM_DELETE_FOLDERS) < 1) {
		reply_error(uid, t, "Error: You are not allowed to delete folders.");
		return 0;
	}

	for(i = 0; i < transaction_object_count(t); i++) {
		switch(transaction_object_id(t, i)) {
			case HL_FILENAME:
				if(file) xfree(file);
				file = transaction_object_string(t, i);
				break;
			case HL_FILEPATH:
				if(path) xfree(path);
				path = transaction_object_path(t, i);
				break;
		}
	}

	if(file == NULL) {
		if(path)
			xfree(path);

		reply_error(uid, t, "Error: No filename specified.");
		return 0;
	}

	if(path != NULL) {
		fp = f_addpath(path);
		xfree(path);

		if(stat(fp, &st) < 0 || !S_ISDIR(st.st_mode)) {
			xfree(file);
			xfree(fp);

			reply_error(uid, t, "Error: Invalid path specified.");
			return 0;
		}

		path = xasprintf("%s/%s", fp, file);
		xfree(fp);
	} else
		path = f_addpath(file);

	xfree(file);

	if(stat(path, &st) < 0) {
		reply_error(uid, t, "Error: Invalid path specified.");
		goto done;
	}

	if(S_ISDIR(st.st_mode)) {
		if(cm_perm_check(uid, HL_PERM_DELETE_FOLDERS) < 1) {
			reply_error(uid, t, "Error: You are not allowed to delete folders.");
			goto done;
		}

		if(f_rmdir_recursive(path) < 0) {
			reply_error(uid, t, "Error: Couldn't completely remove the specified directory. (check permissions or global::allow_recursive_remove)");
			goto done;
		}
	} else {
		if(cm_perm_check(uid, HL_PERM_DELETE_FILES) < 1) {
			reply_error(uid, t, "Error: You are not allowed to delete files.");
			goto done;
		}

		if(unlink(path) < 0) {
			reply_error(uid, t, "Error: Couldn't remove the specified file.");
			goto done;
		}
	}

	reply_success(uid, t);
done:
	xfree(path);
	return 0;
}

int txn_move(int uid, TransactionIn t)
{
	int i;
	char *source = NULL, *dest = NULL;
	char *file = NULL, *sp = NULL, *dp = NULL;
	struct stat st;

	for(i = 0; i < transaction_object_count(t); i++) {
		switch(transaction_object_id(t, i)) {
			case HL_FILENAME:
				if(file) xfree(file);
				file = transaction_object_string(t, i);
				break;
			case HL_FILEPATH:
				if(sp) xfree(sp);
				sp = transaction_object_path(t, i);
				break;
			case HL_DESTPATH:
				if(dp) xfree(dp);
				dp = transaction_object_path(t, i);
		}
	}

	if(!file) {
		reply_error(uid, t, "Error: No source filename specified.");
		goto done;
	}

	if(sp != NULL) {
		source = f_addpath(sp);
		xfree(sp);
		sp = NULL;

		if(stat(source, &st) < 0 || !S_ISDIR(st.st_mode)) {
			reply_error(uid, t, "Error: Invalid source path specified.");
			goto done;
		}

		sp = source;
		source = xasprintf("%s/%s", sp, file);
		xfree(sp);
		sp = NULL;
	} else
		source = f_addpath(file);

	if(stat(source, &st) < 0) {
		reply_error(uid, t, "Error: Invalid source file specified.");
		goto done;
	}

	if(S_ISDIR(st.st_mode)) {
		if(cm_perm_check(uid, HL_PERM_MOVE_FOLDERS) < 1) {
			reply_error(uid, t, "Error: You are not allowed to move folders.");
			goto done;
		}
	} else {
		if(cm_perm_check(uid, HL_PERM_MOVE_FILES) < 1) {
			reply_error(uid, t, "Error: You are not allowed to move files.");
			goto done;
		}
	}

	if(dp != NULL) {
		dest = f_addpath(dp);
		xfree(dp);
		dp = NULL;

		if(stat(dest, &st) < 0 || !S_ISDIR(st.st_mode)) {
			reply_error(uid, t, "Error: Invalid destination path specified.");
			goto done;
		}

		dp = dest;
		dest = xasprintf("%s/%s", dp, file);
		xfree(dp);
		dp = NULL;
	} else 
		dest = f_addpath(file);

	xfree(file);
	file = NULL;

#ifdef DEBUG
	debug("Moving %s to %s", source, dest);
#endif
	if(rename(source, dest) < 0) {
		reply_error(uid, t, "Error: Couldn't move the specified file.");
		goto done;
	}

	reply_success(uid, t);
done:
	if(file) xfree(file);
	if(sp) xfree(sp);
	if(dp) xfree(dp);
	if(source) xfree(source);
	if(dest) xfree(dest);

	return 0;
}

int txn_rename(int uid, TransactionIn t)
{
	int source_index, dest_index, path_index;
	char *source, *dest, *path;
	char *source_path, *dest_path;
	struct stat st;

	if((source_index = transaction_object_index(t, HL_FILENAME)) < 0 || (dest_index = transaction_object_index(t, HL_DESTNAME)) < 0) {
		reply_success(uid, t);
		return 0;
	}

	source = transaction_object_string(t, source_index);
	dest = transaction_object_string(t, dest_index);

	if((path_index = transaction_object_index(t, HL_FILEPATH)) < 0)
		path = NULL;
	else
		path = transaction_object_path(t, path_index);

	source_path = file_path(path, source);
	dest_path = file_path(path, dest);

	xfree(source);
	xfree(dest);

	if(path)
		xfree(path);

	if(source_path == NULL || dest_path == NULL) {
		if(source_path)
			xfree(source_path);

		if(dest_path)
			xfree(dest_path);

		reply_error(uid, t, "Error: Invalid path specified.");
		return 0;
	}

	if(stat(source_path, &st) < 0) {
		xfree(source_path);
		xfree(dest_path);

		reply_error(uid, t, "Error: Invalid path specified.");
		return 0;
	}

	if(S_ISDIR(st.st_mode)) {
		if(cm_perm_check(uid, HL_PERM_RENAME_FOLDERS) < 1) {
			xfree(source_path);
			xfree(dest_path);

			reply_error(uid, t, "Error: You are not allowed to rename folders.");
			return 0;
		}
	} else {
		if(cm_perm_check(uid, HL_PERM_RENAME_FILES) < 1) {
			xfree(source_path);
			xfree(dest_path);

			reply_error(uid, t, "Error: You are not allowed to rename files.");
			return 0;
		}
	}

#ifdef DEBUG
	debug("Renaming %s to %s", source, dest);
#endif

	if(rename(source_path, dest_path) < 0) 
		reply_error(uid, t, "Error: Couldn't rename the specified file.");
	else 
		reply_success(uid, t);

	xfree(source_path);
	xfree(dest_path);

	return 0;
}

int txn_file_info(int uid, TransactionIn t)
{
	int filename_index, filepath_index;
	char *file, *path, *pathname, *type; 
	struct stat st;
	TransactionOut reply;

	if((filename_index = transaction_object_index(t, HL_FILENAME)) < 0) {
		reply_error(uid, t, "Error: No filename specified.");
		return 0;
	}

	file = transaction_object_string(t, filename_index);

	if((filepath_index = transaction_object_index(t, HL_FILEPATH)) < 0)
		path = NULL;
	else
		path = transaction_object_path(t, filepath_index);

	pathname = file_path(path, file); 
	xfree(path);

	if(pathname == NULL || stat(pathname, &st) < 0) {
		if(pathname)
			xfree(pathname);

		xfree(file);

		reply_error(uid, t, "Error: Invalid path specified.");
		return 0;
	}

	xfree(pathname);

	reply = transaction_reply_create(t, 0, 7);

	if(S_ISDIR(st.st_mode)) {
		transaction_add_string(reply, HL_FILE_EXT, "fldr");
		transaction_add_string(reply, HL_FILE_TYPE, "Folder");
		transaction_add_string(reply, HL_FILE_CREATOR, "n/a");
	} else {
		type = f_type(file);
		transaction_add_object(reply, HL_FILE_EXT, type, 4);
		transaction_add_object(reply, HL_FILE_TYPE, type, 4);
		transaction_add_object(reply, HL_FILE_CREATOR, type + 4, 4);
		transaction_add_int32(reply, HL_INFOLEN, st.st_size);
	}

	transaction_add_string(reply, HL_FILENAME, file);
	transaction_add_timestamp(reply, HL_FILE_CTIME, st.st_ctime);
	transaction_add_timestamp(reply, HL_FILE_MTIME, st.st_mtime);
	transaction_write(uid, reply);
	transaction_out_destroy(reply);

	xfree(file);

	return 0;
}

int txn_read_account(int uid, TransactionIn t)
{
	int i;
	char *username = NULL;
	Account a;
	TransactionOut reply;

	if(cm_perm_check(uid, HL_PERM_READ_ACCOUNTS) < 0) {
		reply_error(uid, t, "Error: You are not allowed to read accounts.");
		return 0;
	}

	if((i = transaction_object_index(t, HL_USERNAME)) < 0) {
		reply_error(uid, t, "Error: No username specified.");
		return 0;
	}

	username = transaction_object_string(t, i);

#ifdef DEBUG
	debug("Opening account: %s\n", username);
#endif

	am_lock();
	a = am_lookup_account(username);
	xfree(username);

	if(a == NULL) {
		reply_error(uid, t, "Error: No such user: '%s'", username);
		am_unlock();
		return 0;
	}

	reply = transaction_reply_create(t, 0, 4);
	if(a->realname != NULL) 
		transaction_add_string(reply, HL_NICKNAME, a->realname);
	transaction_add_masked_string(reply, HL_USERNAME, a->username);
	transaction_add_string(reply, HL_PASSWORD, "x");
	transaction_add_object(reply, HL_PERMISSION, a->perms->permissions, 8);
	transaction_write(uid, reply);
	transaction_out_destroy(reply);

	am_unlock();
	return 0;
}

int txn_create_account(int uid, TransactionIn t)
{
	int i;
	char *username, *password, *realname, *p;
	Permissions permissions;

	if(cm_perm_check(uid, HL_PERM_CREATE_ACCOUNTS) < 1) {
		reply_error(uid, t, "Error: You are not allowed to create accounts.");
		return 0;
	}

	if((i = transaction_object_index(t, HL_USERNAME)) < 0) {
		reply_error(uid, t, "Error: No username specified.");
		return 0;
	}

	username = transaction_object_masked_string(t, i);

	if((i = transaction_object_index(t, HL_PERMISSIONS)) >= 0) {
		if((permissions = transaction_object_permissions(t, i)) == NULL) {
			reply_error(uid, t, "Error: Permissions format is invalid.");

			xfree(username);
			return 0;
		}
	} else 
		permissions = permissions_create_from_ascii("60700C2003800000");

	if((i = transaction_object_index(t, HL_NICKNAME)) < 0) 
		realname = xstrdup("");
	else
		realname = transaction_object_string(t, i);


	if((i = transaction_object_index(t, HL_PASSWORD)) < 0) 
		password = NULL;
	else {
		p = transaction_object_masked_string(t, i);
		password = password_encrypt(p);
		xfree(p);
	}

	if(am_add_account_with_permissions(username, realname, password, permissions) < 0)
		reply_error(uid, t, "Error: The account '%s' already exists.", username);
	else {
		am_commit();
		reply_success(uid, t);
	}

	xfree(username);
	xfree(realname);
	permissions_destroy(permissions);

	if(password)
		xfree(password);

	return 0;
}

int txn_modify_account(int uid, TransactionIn t)
{
	int i;
	char *username, *password;
	Account a;

	if(cm_perm_check(uid, HL_PERM_MODIFY_ACCOUNTS) < 1) {
		reply_error(uid, t, "Error: You are not allowed to modify accounts.");
		return 0;
	}

	if((i = transaction_object_index(t, HL_USERNAME)) < 0) {
		reply_error(uid, t, "Error: No account name specified.");
		return 0;
	}

	username = transaction_object_masked_string(t, i);

	am_xlock();
	if((a = am_lookup_account(username)) == NULL) {
		am_xunlock();
		reply_error(uid, t, "Error: No such user: '%s'", username);
		xfree(username);
		return 0;
	}

	xfree(username);

	if((i = transaction_object_index(t, HL_PASSWORD)) >= 0) {
		password = transaction_object_masked_string(t, i);
		if((uint8_t)password[0] == 255 && password[1] == 0) 
			xfree(password);
		else {
			if(a->password != NULL)
				xfree(a->password);

			a->password = password_encrypt(password);
			xfree(password);
		} 
	} else {
		if(a->password != NULL)
			xfree(a->password);

		a->password = NULL;
	}

	if((i = transaction_object_index(t, HL_NICKNAME)) >= 0) {
		if(a->realname != NULL)
			xfree(a->realname);

		a->realname = transaction_object_string(t, i);
	}

	if((i = transaction_object_index(t, HL_PERMISSION)) >= 0) 
		memcpy(a->perms->permissions, transaction_object_data(t, i), 8);

	am_xunlock();
	am_commit();

	reply_success(uid, t);
	return 0;
}

int txn_delete_account(int uid, TransactionIn t)
{
	int i;
	char *username;

	if(cm_perm_check(uid, HL_PERM_DELETE_ACCOUNTS) < 1) {
		reply_error(uid, t, "Error: You are not allowed to delete accounts.");
		return 0;
	}

	if((i = transaction_object_index(t, HL_USERNAME)) < 0) {
		reply_error(uid, t, "Error: No username specified.");
		return 0;
	}

	username = transaction_object_masked_string(t, i);
	if(am_del_account(username) < 0)
		reply_error(uid, t, "Error: No such user: '%s'");
	else {
		am_commit();
		reply_success(uid, t);
	}

	return 0;
}

int txn_admin_accounts(int uid, TransactionIn t)
{
	TransactionOut reply;

	reply = am_build_userlist(t);
	transaction_write(uid, reply);
	transaction_out_destroy(reply);

	return 0;
}

static int process_user_entry(uint8_t *buf, int len)
{
	int ret = -1, null_password = 1;
	int i, j;
	int16_t n, id, size, t16;
	char *source_username = NULL;
	char *username = NULL; 
	char *realname = NULL; 
	char *password = NULL;
	char *t;
	Permissions permissions = NULL;
	Account a;

	memcpy(&t16, buf, 2);
	n = ntohs(t16);

	buf += 2;
	len -=2;

	for(i = 0; i < n; i++) {
		if(len < 4)
			goto done;

		memcpy(&t16, buf, 2);
		id = ntohs(t16);

		memcpy(&t16, buf + 2, 2);
		size = ntohs(t16);

		buf += 4;
		len -= 4;

		if(len < size)
			goto done;

		switch(id) {
			case HL_MESSAGE:
				if(source_username)
					goto done;

				source_username = (char *)xmalloc(size + 1);
				for(j = 0; j < size; j++)
					source_username[j] = buf[j] ^ 0xFF;
				source_username[size] = '\0';

#ifdef DEBUG
				debug("HL_MESSAGE: %s", source_username);
#endif
				break;
			case HL_USERNAME:
				if(username)
					goto done;

				username = (char *)xmalloc(size + 1);
				for(j = 0; j < size; j++)
					username[j] = buf[j] ^ 0xFF;
				username[size] = '\0';

#ifdef DEBUG
				debug("HL_USERNAME: %s", username);
#endif
				break;
			case HL_NICKNAME:
				if(realname)
					goto done;

				realname = (char *)xmalloc(size + 1);
				memcpy(realname, buf, size);
				realname[size] = '\0';

#ifdef DEBUG
				debug("HL_NICKNAME: %s", realname);
#endif
				break;
			case HL_PASSWORD:
				if(password)
					goto done;

				null_password = 0;
				if(size == 1 && *buf == 0)
					break;

				t = (char *)xmalloc(size + 1);
				for(j = 0; j < size; j++)
					t[j] = buf[j] ^ 0xFF;
				t[size] = '\0';

				password = password_encrypt(t);
				xfree(t);

#ifdef DEBUG
				debug("HL_PASSWORD: %s", password);
#endif
				break;
			case HL_PERMISSION:
				if(permissions)
					goto done;

				if(size != 8)
					goto done;

				permissions = permissions_create_from_binary(buf);
#ifdef DEBUG
				debug("HL_PERMISSIONS");
#endif
		}

		buf += size;
		len -= size;
	}

	if(source_username && !realname && !password && !permissions && !username) {
		am_del_account(source_username);

		ret = 0;
		goto done;
	}


	am_xlock();

	if(source_username) {
		if(!username)
			am_del_account(source_username);
		else {

			if((a = am_lookup_account(source_username)) == NULL)
				goto done;

			if(!null_password && !password) {
				password = a->password;
				a->password = NULL;
			}

			if(!realname) {
				realname = a->realname;
				a->realname = NULL;
			}

			if(!permissions) {
				permissions = a->perms;
				a->perms = NULL;
			}

			am_xunlock();

			am_del_account(source_username);
			am_add_account_with_permissions(username, realname, password, permissions);
		}
	} else { 
		am_xunlock();

		if(!username)
			goto done;

		if(!realname)
			realname = xstrdup("");

		if(!permissions)
			permissions = permissions_create_from_ascii("60700C2003800000");

		am_add_account_with_permissions(username, realname, password, permissions);
	} 

	ret = 0;
done:
	if(source_username)
		xfree(source_username);

	if(username)
		xfree(username);

	if(realname)
		xfree(realname);

	if(password)
		xfree(password);

	if(permissions)
		permissions_destroy(permissions);

	return ret;
}

int txn_admin_submit(int uid, TransactionIn t)
{
	int i;

	if(cm_perm_check(uid, HL_PERM_MODIFY_ACCOUNTS) < 1) {
		reply_error(uid, t, "Error: You are not allowed to modify accounts.");
		return 0;
	}

	for(i = 0; i < transaction_object_count(t); i++) {
		if(transaction_object_id(t, i) == HL_MESSAGE) {
			if(process_user_entry(transaction_object_data(t, i), transaction_object_len(t, i)) < 0) {
				reply_error(uid, t, "Error: Malformed data.");
				return 0;
			}
		}	
	}

	am_commit();

	reply_success(uid, t);
	return 0;
}

int txn_kick_user(int uid, TransactionIn t)
{
	int i, r;
	char *s;
	uint16_t uuid;
	Permissions p;
	
	if(cm_perm_check(uid, HL_PERM_DISCONNECT_USERS) < 1) {
		reply_error(uid, t, "Error: You are not allowed to disconnect users.");
		return 0;
	}

	if((i = transaction_object_index(t, HL_SOCKETNO)) < 0) {
		reply_error(uid, t, "Error: No UID specified.");
		return 0;
	}

	uuid = transaction_object_int16(t, i);

	r = cm_perm_check(uuid, HL_PERM_UNDISCONNECTABLE);
	if(r < 0 || uid == uuid) {
		reply_error(uid, t, "Error: Invalid user specified.");
		return 0;
	}

	if(!r) {
		/* Superusers can disconnect anyone */
		p = cm_perm_getall(uid);
		r = permissions_is_superuser(p);
		xfree(p);

		if(!r) { 
			reply_error(uid, t, "Error: You are not allowed to disconnect the specified user"); 
			return 0;
		}
	}

	if(!cm_getval(uid, CONN_NICKNAME, &s)) { 
		message_error(uuid, "Kicked by %s", s);
		xfree(s);
	}

	tfm_purge_uid(uuid);
	cm_transaction_broadcast(uuid, txn_part_create(uuid));
	cm_remove(uuid);

	reply_success(uid, t);
	return 0;
}

int txn_user_info(int uid, TransactionIn t)
{
        if(cm_perm_check(uid, HL_PERM_GET_USER_INFO) < 1) {
                reply_error(uid, t, "Error: You are not allowed to view user information.");
                return 0;
        }

        reply_error(uid, t, "Error: This server doesn't support viewing user information.");

        return 0;
}

int txn_broadcast(int uid, TransactionIn t)
{
	int i;
	char *message, *nickname;
	TransactionOut mtxn;
	
	if(cm_perm_check(uid, HL_PERM_BROADCAST) < 1) {
		reply_error(uid, t, "Error: You are not allowed to send broadcast messages.");
		return 0;
	}

	if(cm_getval(uid, CONN_NICKNAME, &nickname) < 0) 
		return -1;
		
	if((i = transaction_object_index(t, HL_MESSAGE)) < 0)
		return 0;

	message = transaction_object_string(t, i);

	mtxn = transaction_create(HL_BROADCAST, 3);
	transaction_add_int16(mtxn, HL_SOCKETNO, uid);
	transaction_add_string(mtxn, HL_NICKNAME, nickname);
	transaction_add_string(mtxn, HL_MESSAGE, message);

	xfree(nickname);
	xfree(message);

	cm_transaction_broadcast(uid, mtxn);
	reply_success(uid, t);

	return 0;
}

int txn_send_chat(int uid, TransactionIn t)
{
	int i;
	char *message, *nickname, *outbuf;
	TransactionOut mtxn;

	if(cm_perm_check(uid, HL_PERM_SEND_CHAT) < 1) 
		return 0;

	if(cm_getval(uid, CONN_NICKNAME, &nickname) < 0)
		return -1;

	if((i = transaction_object_index(t, HL_MESSAGE)) < 0) 
		return 0;

	if(transaction_object_len(t, i) > MAX_CHAT_MESSAGE_LEN) 
		return 0;

	if(cm_getval(uid, CONN_NICKNAME, &nickname) < 0) 
		return -1;

	message = transaction_object_string(t, i);

	if((i = transaction_object_index(t, HL_EMOTE)) >= 0 && transaction_object_int16(t, i)) 
		outbuf = xasprintf("\015* %s %s", nickname, message);
	else
		outbuf = xasprintf("\015<%s> %s", nickname, message);


	xfree(message);
	xfree(nickname);

	mtxn = transaction_create(HL_RELAYCHAT, 1);
	transaction_add_string(mtxn, HL_MESSAGE, outbuf);
	xfree(outbuf);

	cm_transaction_broadcast(-1, mtxn);

	return 0;
}

int txn_send_privmsg(int uid, TransactionIn t)
{
        if(cm_perm_check(uid, HL_PERM_SEND_MESSAGES) < 1) {
                reply_error(uid, t, "Error: You are not allowed to send private messages.");
                return 0;
        }

        reply_error(uid, t, "Error: This server doesn't support private messages.");

        return 0;
}

int txn_request_chat(int uid, TransactionIn t)
{
        if(cm_perm_check(uid, HL_PERM_START_CHAT) < 1) {
                reply_error(uid, t, "Error: You are not allowed to initiate private chat.");
                return 0;
        }

        reply_error(uid, t, "Error: This server doesn't support private chat.");

        return 0;
}

int txn_get_news_bundle(int uid, TransactionIn t)
{
	if(cm_perm_check(uid, HL_PERM_READ_ARTICLES) < 1) {
		reply_error(uid, t, "Error: You are not allowed to read news articles.");
		return 0;
	}

	reply_error(uid, t, "Error: This server doesn't support news.");

	return 0;
}

int txn_ping(int uid, TransactionIn t)
{
	reply_success(uid, t);
	return 0;
}
