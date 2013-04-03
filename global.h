#ifndef GLOBAL_H
#define GLOBAL_H

#include <sys/types.h>
#include <inttypes.h>

#ifndef NULL
#define NULL	0
#endif

/* Program information */
#define PACKAGE		"hotwired"
#define VERSION		"0.0.1"

/* Default port to bind to */
#define HL_DEFAULT_PORT         5500

/* Limits for the thread pool */
#define DEFAULT_MAX_ACTIVE_THREADS      128
#define DEFAULT_MAX_INACTIVE_THREADS    8

/* Version the server is supposedly emulating (in this case, 1.8.5) */
#define SERVER_VERSION  	185

/* These are sanity limits for incoming transaction sizes */
#define MAX_TXN_SIZE            262144
#define MAX_TXN_OBJS            64

/* Number of seconds to wait for a response from a client */
#define RESPONSE_TIMEOUT        30

/* Number of seconds to wait for a connection on the data port */
#define DATA_CONNECT_TIMEOUT    30

/* Maximum length of a chat message */
#define MAX_CHAT_MESSAGE_LEN	512

#endif
