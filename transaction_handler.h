#ifndef TRANSACTION_HANDLER_H
#define TRANSACTION_HANDLER_H

#include <Transaction.h>

void th_init();
int th_exec(int uid, TransactionIn t);

/* Transaction handlers */
int txn_login(int uid, TransactionIn t);
int txn_info(int uid, TransactionIn t);
int txn_userlist(int uid, TransactionIn t);
int txn_filelist(int uid, TransactionIn t);
int txn_download(int uid, TransactionIn t);
int txn_upload(int uid, TransactionIn t);
int txn_download_folder(int uid, TransactionIn t);
int txn_upload_folder(int uid, TransactionIn t);
int txn_xfer_cancel(int uid, TransactionIn t);
int txn_create_folder(int uid, TransactionIn t);
int txn_delete(int uid, TransactionIn t);
int txn_move(int uid, TransactionIn t);
int txn_rename(int uid, TransactionIn t);
int txn_file_info(int uid, TransactionIn t);
int txn_read_account(int uid, TransactionIn t);
int txn_create_account(int uid, TransactionIn t);
int txn_modify_account(int uid, TransactionIn t);
int txn_delete_account(int uid, TransactionIn t);
int txn_admin_accounts(int uid, TransactionIn t);
int txn_admin_submit(int uid, TransactionIn t);
int txn_kick_user(int uid, TransactionIn t);
int txn_user_info(int uid, TransactionIn t);
int txn_broadcast(int uid, TransactionIn t);
int txn_send_chat(int uid, TransactionIn t);
int txn_send_privmsg(int uid, TransactionIn t);
int txn_request_chat(int uid, TransactionIn t);
int txn_get_news_bundle(int uid, TransactionIn t);
int txn_ping(int uid, TransactionIn t);
	
#endif
