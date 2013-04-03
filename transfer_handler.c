#include <global.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <Config.h>
#include <MQueue.h>
#include <TransferManager.h>
#include <log.h>
#include <machdep.h>
#include <output.h>
#include <socketops.h>
#include <transfer_handler.h>
#include <util.h>
#include <xmalloc.h>

#ifdef DEBUG
#include <ctype.h>
#include <stdio.h>
#endif

/* Size of transfer buffers (Must be >= 256) */
#define BUFFER_SIZE	512

/* How long to wait for a data socket to close (in seconds) */
#define CLOSE_TIMEOUT	30

/* XXX Hotline has this propensity to litter everything with a bunch of
   values which seem to do nothing and are always zero.  So, the header
   starts zeroed and the values that actually seem to hold some significance
   are initialized.  I've picked through this header in a sniffer again and 
   as far as I can tell this should be complete, but Pitbull Pro is complaining
   that the header is malformed.  If you see any inconsistancies in the
   contruction of this header pleace inform me.
 */
static void xfer_dl_header(int fd, char *filename, int length)
{
	int l;
	uint8_t *header;

	l = strlen(filename);

	header = (uint8_t *)xmalloc(130 + l);
	memset(header, '\0', 130 + l);

	/* File transfer header */
	memcpy(header, "FILP", 4);

	/* Unknown value */
	mcpy_int16(header + 4, 1);

	/* Number of sections in header? */
	mcpy_int16(header + 22, 2);

	/* Beginning of INFO section */
	memcpy(header + 24, "INFO", 4);

	/* Length of MWIN section? */
	mcpy_int32(header + 36, 74 + l);

	/* Beginning of MWIN section */
	memcpy(header + 40, "MWIN", 4);

	/* File type */
	memcpy(header + 44, "BINAdosa", 8);

	/* Length of filename */
	mcpy_int32(header + 106, l);

	/* Filename */
	memcpy(header + 112, filename, l);

	/* Beginning of DATA section */
	memcpy(header + 114 + l, "DATA", 4);

	/* Number of bytes to be transfered */
	mcpy_int32(header + 126 + l, length);

	write_all(fd, header, 130 + l);
	xfree(header);
}

#ifdef DEBUG
static void display_data(uint8_t *buf, int len)
{
	int i;
	fprintf(stderr, "Data: ");

	for(i = 0; i < len; i++) {
		if(isalpha(buf[i]))
			fprintf(stderr, "%c ", buf[i]);
		else
			fprintf(stderr, "%u ", (unsigned int)buf[i]);
	}

	fputc('\n', stderr);
}
#endif

/* XXX Most of the values in this header are unknown and therefore ignored */
static void xfer_upload(int sock, int tid, TransferInfo d)
{
	int fd, umask, n;
	char *filename;
	uint32_t len;
	uint8_t buf[BUFFER_SIZE];

	filename = xasprintf("%s.hpf", d->filename);

#ifdef DEBUG
	debug("Starting upload: %s (offset: %d)", d->filename, d->offset);
	debug("to file: %s", filename);
#endif
	if(read_all(sock, buf, 108) < 0)
		goto done;

#ifdef DEBUG
	display_data(buf, 108);
#endif

	if(strncmp((char *)buf, "FILP", 4) < 0)
		goto done;

	if(read_int32(sock, &len) < 0)
		goto done;

#ifdef DEBUG
	debug("Remaining block length: %d", len);
#endif

	if(len > 242) {
#ifdef DEBUG
		debug("Received malformed transfer header with bogus length value");
#endif
		goto done;
	}

	if(read_all(sock, buf, len + 14) < 0)
		goto done;

#ifdef DEBUG
	display_data(buf, len + 14);
#endif

	if(read_int32(sock, &len) < 0)
		goto done;

#ifdef DEBUG
	debug("File size: %d bytes", len);
#endif

	if(!d->mode) {
		if((umask = config_int_value("transfers", "file_umask")) < 0)
			umask = 0644;

		if((fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, umask)) < 0) 
			goto done;
	} else {
		if((fd = open(filename, O_WRONLY)) < 0)
			goto done;

		lseek(fd, d->offset, SEEK_SET);
	}

	log("*** Beginning upload (TID: %d) to %s (%d bytes)", tid, d->filename, len);

	while(len > 0) {
		n = BUFFER_SIZE > len ? len : BUFFER_SIZE;

		if(read_all(sock, buf, n) < 0) 
			goto err;

		if(write(fd, buf, n) < 0) 
			goto err;

		len -= n;
	}

	close(fd);
	link(filename, d->filename);
	unlink(filename);

	log("*** Upload (TID: %d) to %s completeted successfully", tid, d->filename);

	goto done;
err:
	log("*** Upload (TID: %d) to %s failed.", tid, d->filename);
	close(fd);

	if(config_truth_value("transfers", "remove_partials") == 1)
		unlink(filename);
done:
	xfree(filename);
	xfree(d->filename);
	xfree(d);

	tfm_remove(tid);
}

static void xfer_download(int sock, int tid, TransferInfo d)
{
	/* Variables for a file transfer */
	int fd, n, len;
	struct stat st;
	char buf[BUFFER_SIZE];

	/* Variables for close timeout */
	fd_set fdset;
	struct timeval tv;
	
#ifdef DEBUG
	debug("Beginning download...");
#endif
	if((fd = open(d->filename, O_RDONLY)) < 0)
		goto done;

	if(fstat(fd, &st) < 0)
		goto done;

	if((len = st.st_size - d->offset) < 0)
		goto done;

	log("*** Beginning download (TID: %d) from %s (%d bytes)", tid, d->filename, len);

	if(!d->mode)
		xfer_dl_header(sock, d->filename, len);

	if(d->offset)
		lseek(fd, d->offset, SEEK_SET);

	while(len > 0) {
		if((n = read(fd, buf, BUFFER_SIZE > len ? len : BUFFER_SIZE)) < 1)
			goto done;

		if(write_all(sock, buf, n) < 0)
			goto done;

		len -= n;
	}

	close(fd);

	/* I believe there is a race condition in the official Hotline client
	   wherein even if all the bytes for a given transfer have been
	   written to the data socket, if the connection is closed too soon
	   the client will believe the transwer was prematurely terminated.
	   In order to solve this, we wait a specified amount of time for
	   the connection to close */

	tv.tv_sec = CLOSE_TIMEOUT;
	tv.tv_usec = 0;

	FD_ZERO(&fdset);
	FD_SET(sock, &fdset);

	select(sock + 1, &fdset, NULL, NULL, &tv);
done:
	log("*** Download (TID: %d) from %s completed", tid, d->filename);

	xfree(d->filename);
	xfree(d);

	tfm_remove(tid);
}

void xfer_wait(int fd, int tid)
{
	MQBuf b;

	b = mqbuf_create();
	mqueue_recv(b);
	switch(mqbuf_message_id(b)) {
		case HT_UPLOAD:
			xfer_upload(fd, tid, (TransferInfo)mqbuf_message_ptrval(b));
			break;
		case HT_DOWNLOAD:
			xfer_download(fd, tid, (TransferInfo)mqbuf_message_ptrval(b));
	}
	mqbuf_destroy(b);
}
