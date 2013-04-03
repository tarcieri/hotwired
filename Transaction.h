#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <global.h>
#include <Permissions.h>

typedef struct _TransactionIn *TransactionIn;
typedef struct _TransactionOut *TransactionOut;

TransactionIn transaction_read(int fd);
void transaction_in_destroy(TransactionIn);
uint16_t transaction_id(TransactionIn t);
uint32_t transaction_taskno(TransactionIn t);
int transaction_object_count(TransactionIn t);
int transaction_object_index(TransactionIn t, uint16_t type);
uint16_t transaction_object_id(TransactionIn t, uint16_t objno);
int transaction_object_len(TransactionIn t, uint16_t objno);
void *transaction_object_data(TransactionIn t, uint16_t objno);
uint16_t transaction_object_int16(TransactionIn t, uint16_t objno);
uint32_t transaction_object_int32(TransactionIn t, uint16_t objno);
char *transaction_object_string(TransactionIn t, uint16_t objno);
char *transaction_object_masked_string(TransactionIn t, uint16_t objno);
char *transaction_object_path(TransactionIn t, uint16_t objno);
Permissions transaction_object_permissions(TransactionIn t, uint16_t objno);
	
TransactionOut transaction_create(uint16_t tid, uint16_t objects);
TransactionOut transaction_reply_create(TransactionIn t, uint32_t errorcode, uint16_t objects);
void transaction_out_destroy(TransactionOut);
void transaction_add_object(TransactionOut t, uint16_t type, void *buf, uint16_t len);
void transaction_add_int16(TransactionOut t, uint16_t type, uint16_t value);
void transaction_add_int32(TransactionOut t, uint16_t type, uint32_t value);
void transaction_add_string(TransactionOut t, uint16_t type, char *string);
void transaction_add_masked_string(TransactionOut t, uint16_t type, char *string);
void transaction_add_timestamp(TransactionOut t, uint16_t type, time_t timestamp);
int transaction_write(int fd, TransactionOut t);

#ifdef DEBUG
void transaction_print(TransactionIn t);
#endif
	
#endif
