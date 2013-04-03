#ifndef MQUEUE_H
#define MQUEUE_H

/* Datagram identifiers */
enum {
        HT_KILL,
        HT_CONTROL_CONNECT,
        HT_DATA_CONNECT,
        HT_TRANSACTION,
	HT_DOWNLOAD,
	HT_UPLOAD
};

typedef struct _MQueue *MQueue;
typedef struct _MQBuf *MQBuf;

#include <global.h>

MQueue mqueue_create();
void mqueue_destroy(MQueue q);

void mqueue_init(MQueue q);
MQueue mqueue_self();

MQBuf mqbuf_create();
void mqbuf_destroy(MQBuf b);

void mqueue_send_ptr(MQueue q, uint16_t id, void *data);
void mqueue_send_int(MQueue q, uint16_t id, int val);
void mqueue_send(MQueue q, uint16_t id);
void mqueue_reply_ptr(MQBuf b, uint16_t id, void *data);
void mqueue_reply_int(MQBuf b, uint16_t id, int val);
void mqueue_reply(MQBuf b, uint16_t id);
void mqueue_recv(MQBuf b);

uint16_t mqbuf_message_id(MQBuf b);
void *mqbuf_message_ptrval(MQBuf b);
int mqbuf_message_intval(MQBuf b);

#endif
