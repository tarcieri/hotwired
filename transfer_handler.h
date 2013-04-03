#ifndef TRANSFER_HANDLER_H
#define TRANSFER_HANDLER_H

typedef struct _TransferInfo {
	char *filename;
	int mode;
	int offset;
} *TransferInfo;

void xfer_wait(int fd, int tid);
	
#endif
