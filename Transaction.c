#include <global.h>

#include <sys/uio.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <ConnectionManager.h>
#include <Permissions.h>
#include <Transaction.h>
#include <machdep.h>
#include <socketops.h>
#include <xmalloc.h>

#ifdef DEBUG
#include <stdio.h>
#include <ctype.h>
#include <output.h>
#endif

struct TransactionObject {
	uint16_t obj_id;
	uint16_t obj_len;
	void *obj_data;
};

struct _TransactionIn {
	uint16_t t_class;
	uint16_t t_id;
	uint32_t t_taskno;
	uint32_t t_error;

	uint8_t *buffer;
	struct TransactionObject *object;
	uint16_t obj_count;
};

struct _TransactionOut {
	uint16_t t_class;
	uint16_t t_id;
	uint32_t t_taskno;
	uint32_t t_error;
	uint32_t length;

	uint16_t obj_count, max_objects;
	struct iovec *iov;
	uint8_t header[22];
};

/* Read an incoming transaction and create a transaction object.  Zero copy */
TransactionIn transaction_read(int fd)
{
	TransactionIn t;
	uint8_t header[22], *bptr;
	uint16_t max_objects;
	uint32_t length;

#ifndef HOST_BIGENDIAN
	uint16_t v16;
#endif

	if(read_all(fd, header, 22) < 0) {
#ifdef DEBUG
		debug("*** TXN_ERR: read() error while reading header");
#endif
		return NULL;
	}

	t = NEW(TransactionIn);

	/* Hopefully these are aligned properly as not to cause bus errors.
	   Otherwise this whole thing may need to be rewritten with memcpys */
	t->t_class = ntohs(*(uint16_t *)header);
	t->t_id = ntohs(*(uint16_t *)(header + 2));
	t->t_taskno = ntohl(*(uint32_t *)(header + 4));
	t->t_error = ntohl(*(uint32_t *)(header + 8));
	length = ntohl(*(uint32_t *)(header + 12)) - 2;
	max_objects = ntohs(*(uint16_t *)(header + 20));

#ifdef DEBUG
	debug("--- | INCOMING TRANSACTION | ---");
	debug("Length:  %d", length);
	debug("Objects: %d", max_objects);
#endif

	if(length > MAX_TXN_SIZE || max_objects > MAX_TXN_OBJS) {
#ifdef DEBUG
		debug("*** TXN_ERR: Too long or too many objects");
#endif
		goto err0;
	}

	if(max_objects == 0 || length == 0) {
		t->buffer = NULL;
		t->object = NULL;
		t->obj_count = 0;

		return t;
	}

	t->buffer = (uint8_t *)xmalloc(length);
	if(read_all(fd, t->buffer, length) < 0) {
#ifdef DEBUG
		debug("*** TXN_ERR: read() error reading body");
#endif
		goto err1;
	}

	t->object = (struct TransactionObject *)
		xmalloc(max_objects * sizeof(struct TransactionObject));

	for(t->obj_count = 0, bptr = t->buffer; t->obj_count < max_objects; t->obj_count++) {
		if(length < 4) {
#ifdef DEBUG
			debug("*** TXN_ERR: Length mismatch in object header");
#endif
			goto err2;
		}

#if HOST_BIGENDIAN
		memcpy(&t->object[t->obj_count].obj_id, bptr, 2);
#else
		memcpy(&v16, bptr, 2);
		t->object[t->obj_count].obj_id = ntohs(v16);
#endif

		bptr += 2;
		length -= 2;

#if HOST_BIGENDIAN
		memcpy(&t->object[t->obj_count].obj_len, bptr, 2);
#else
		memcpy(&v16, bptr, 2);
		t->object[t->obj_count].obj_len = ntohs(v16);
#endif

		bptr += 2;
		length -= 2;

		if(length < t->object[t->obj_count].obj_len) {
#ifdef DEBUG
			debug("*** TXN_ERR: Length mismatch in object body");
#endif
			goto err2;
		}

#ifdef DEBUG
		debug("Object length is %d", t->object[t->obj_count].obj_len);
#endif
		t->object[t->obj_count].obj_data = bptr;

		bptr += t->object[t->obj_count].obj_len;
		length -= t->object[t->obj_count].obj_len;
	}

#ifdef DEBUG
	debug("*** Got transaction: %p", t);
#endif

	return t;

err2:
	xfree(t->object);
err1:
	xfree(t->buffer);
err0:
	xfree(t);
	return NULL;
}

void transaction_in_destroy(TransactionIn t)
{
	xfree(t->object);
	xfree(t->buffer);
	xfree(t);
}

uint16_t transaction_id(TransactionIn t)
{
	return t->t_id;
}

uint32_t transaction_taskno(TransactionIn t)
{
	return t->t_taskno;
}

int transaction_object_count(TransactionIn t)
{
	return t->obj_count;
}

int transaction_object_index(TransactionIn t, uint16_t type)
{
	int i;

	for(i = 0; i < t->obj_count; i++)
		if(t->object[i].obj_id == type)
			return i;

	return -1;
}

uint16_t transaction_object_id(TransactionIn t, uint16_t objno)
{
	return t->object[objno].obj_id;
}

int transaction_object_len(TransactionIn t, uint16_t objno)
{
	return t->object[objno].obj_len;
}

void *transaction_object_data(TransactionIn t, uint16_t objno)
{
	return t->object[objno].obj_data;
}

uint16_t transaction_object_int16(TransactionIn t, uint16_t objno)
{
	uint16_t ret;

	if(t->object[objno].obj_len >= 2) {
		memcpy(&ret, t->object[objno].obj_data, 2);
		return ntohs(ret);
	} else if(t->object[objno].obj_len > 0)
		return *(uint8_t *)t->object[objno].obj_data;

	return 0;
}

uint32_t transaction_object_int32(TransactionIn t, uint16_t objno)
{
	uint16_t ret16;
	uint32_t ret32;

	if(t->object[objno].obj_len >= 4) {
		memcpy(&ret32, t->object[objno].obj_data, 4);
		return ntohl(ret32);
	} else if(t->object[objno].obj_len >= 2) {
		memcpy(&ret16, t->object[objno].obj_data, 2);
		return ntohs(ret16);
	} else if(t->object[objno].obj_len > 0)
		return *(uint8_t *)t->object[objno].obj_data;
	
	return 0;
}

char *transaction_object_string(TransactionIn t, uint16_t objno)
{
	char *ret = (char *)xmalloc(t->object[objno].obj_len + 1);
	memcpy(ret, t->object[objno].obj_data, t->object[objno].obj_len);
	ret[t->object[objno].obj_len] = '\0';

	return ret;
}

char *transaction_object_masked_string(TransactionIn t, uint16_t objno)
{
	int i;
	char *ret = (char *)xmalloc(t->object[objno].obj_len + 1);

	for(i = 0; i < t->object[objno].obj_len; i++)
		ret[i] = ((uint8_t *)t->object[objno].obj_data)[i] ^ 0xFF;
	ret[t->object[objno].obj_len] = '\0';

	return ret;
}

/* This function uses a two pass process to convert to standard paths */
char *transaction_object_path(TransactionIn t, uint16_t objno)
{
	char *ret;
	int i, j, l, ret_len = 0, obj_len = t->object[objno].obj_len;
	int8_t entry_len;
	int16_t entries;
	uint8_t *data = (uint8_t *)t->object[objno].obj_data;

	if(obj_len < 2)
		return NULL;

	memcpy(&entries, data, 2);
	entries = ntohs(entries);
	data += 2;
	obj_len -= 2;

	if(entries == 0)
		return NULL;

	/* Pass 1: Check the path for consistency */
	for(i = 0, l = 0; i < entries; i++) {
		if(obj_len - l < 3)
			return NULL;

		entry_len = data[l + 2];
		if(obj_len - l < entry_len + 3)
			return NULL;

		/* Disallow '..' as a valid entry */
		if(entry_len == 2 && !strncmp((char *)data + l + 3, "..", 2))
			return NULL;
		
		/* Dusallow '/' in path elements */
		for(j = 0; j < entry_len; j++)
			if(data[l + 3 + j] == '/')
				return NULL;
		
		ret_len += entry_len + 1;
		l += entry_len + 3;
	}

	ret = (char *)xmalloc(ret_len + 1);

	/* Pass 2: Construct the output string */
	for(i = 0, l = 0; i < entries; i++) {
		entry_len = data[2];
		memcpy(ret + l, data + 3, entry_len);
		ret[l + entry_len] = '/';
		data += entry_len + 3;
		l += entry_len + 1;
	}

	ret[l] = '\0';

#ifdef DEBUG
	debug("Output path is: %s (%d : %d)", ret, strlen(ret), ret_len);
#endif

	return ret;
}

Permissions transaction_object_permissions(TransactionIn t, uint16_t objno)
{
	if(t->object[objno].obj_len != 8)
		return NULL;

	return permissions_create_from_binary(transaction_object_data(t, objno));
}

#ifdef DEBUG
void transaction_print(TransactionIn t)
{
	int i, j;
	uint8_t *data;

	debug("Transaction type: %d\t(Class: %d Error: %d Task #%d)", t->t_id, t->t_class, t->t_error, t->t_taskno);
	debug("Transaction contains %d objects", t->obj_count);
	for(i = 0; i < t->obj_count; i++) {
		debug("Object %d: Type %d (Length %d)", i, t->object[i].obj_id, t->object[i].obj_len);

		if(t->object[i].obj_len == 2) 
			debug("Data: %d", (int)transaction_object_int16(t, i));
		else if(t->object[i].obj_len == 4)
			debug("Data: %d", (int)transaction_object_int32(t, i));
		else {
			data = t->object[i].obj_data;
			fprintf(stderr, "Data: ");

			for(j = 0; j < t->object[i].obj_len; j++) {
				if(isalpha(data[j]))
					fprintf(stderr, "%c ", data[j]);
				else
					fprintf(stderr, "%u ", (unsigned int)data[j]);
			}

			fputc('\n', stderr);
		}
	}
}
#endif

static void transaction_iov_init(TransactionOut t)
{
	t->iov = (struct iovec *)xmalloc((7 + t->max_objects) * sizeof(struct iovec));

	t->iov[0].iov_len = 2;
	t->iov[0].iov_base = (char *)&t->t_class;

	t->iov[1].iov_len = 2;
	t->iov[1].iov_base = (char *)&t->t_id;

	t->iov[2].iov_len = 4;
	t->iov[2].iov_base = (char *)&t->t_taskno;

	t->iov[3].iov_len = 4;
	t->iov[3].iov_base = (char *)&t->t_error;
}

TransactionOut transaction_create(uint16_t tid, uint16_t objects)
{
	TransactionOut t = NEW(TransactionOut);

	t->t_class = 0;
#if HOST_BIGENDIAN
	t->t_id = tid;
#else
	t->t_id = htons(tid);
#endif
	t->t_error = 0;

	t->length = 2;
	t->obj_count = 0;
	t->max_objects = objects;

	transaction_iov_init(t);

	return t;
}

TransactionOut transaction_reply_create(TransactionIn t, uint32_t errorcode, uint16_t objects)
{
	TransactionOut ret = NEW(TransactionOut);

	ret->t_class = htons(1);
	ret->t_taskno = htonl(t->t_taskno);
	ret->t_error = htonl(errorcode);
	ret->t_id = 0;

	ret->length = 2;
	ret->obj_count = 0;
	ret->max_objects = objects;

	transaction_iov_init(ret);

	return ret;
}

void transaction_out_destroy(TransactionOut t)
{
	int i;

	for(i = 0; i < t->obj_count; i++)
		xfree(t->iov[7 + i].iov_base);

	xfree(t->iov);
	xfree(t);
}

void transaction_add_object(TransactionOut t, uint16_t type, void *buf, uint16_t len)
{
#ifndef HOST_BIGENDIAN
	uint16_t v16;
#endif
	uint8_t *obj_buf;

	if(t->obj_count >= t->max_objects)
		return;

	obj_buf = (uint8_t *)xmalloc(4 + len);

#if HOST_BIGENDIAN
	memcpy(obj_buf, &type, 2);
	memcpy(obj_buf + 2, &len, 2);
#else
	v16 = htons(type);
	memcpy(obj_buf, &v16, 2);
	v16 = htons(len);
	memcpy(obj_buf + 2, &v16, 2);
#endif

	if(len > 0) 
		memcpy(obj_buf + 4, buf, len);

	t->iov[t->obj_count + 7].iov_len = 4 + len;
	t->iov[t->obj_count + 7].iov_base = (char *)obj_buf;
	t->length += 4 + len;

	t->obj_count++;
}

void transaction_add_int16(TransactionOut t, uint16_t type, uint16_t value)
{
#if HOST_BIGENDIAN
	transaction_add_object(t, type, &value, 2);
#else
	uint16_t v = htons(value);
	transaction_add_object(t, type, &v, 2);
#endif
}

void transaction_add_int32(TransactionOut t, uint16_t type, uint32_t value)
{
#if HOST_BIGENDIAN
	transaction_add_object(t, type, &value, 4);
#else
	uint32_t v = htonl(value);
	transaction_add_object(t, type, &v, 4);
#endif
}

void transaction_add_string(TransactionOut t, uint16_t type, char *string)
{
	transaction_add_object(t, type, string, strlen(string));
}

void transaction_add_masked_string(TransactionOut t, uint16_t type, char *string)
{
	int i, l;
	uint8_t *buf;

	l = strlen(string);
	buf = (uint8_t *)xmalloc(l);
	
	for(i = 0; i < l; i++)
		buf[i] = string[i] ^ 0xFF;

	transaction_add_object(t, type, buf, l);
	xfree(buf);
}

void transaction_add_timestamp(TransactionOut t, uint16_t type, time_t timestamp)
{
	struct tm tm;
	uint8_t ts[8];
	int16_t year;
	int32_t date;

	localtime_r(&timestamp, &tm);
	year = htons(1900 + tm.tm_year);
	date = htonl(tm.tm_sec + tm.tm_min * 60 + tm.tm_hour * 3600 + tm.tm_yday * 86400);

	memset(ts, 0, 8);
	memcpy(ts, &year, 2);
	memcpy(ts + 4, &date, 4);

	transaction_add_object(t, type, ts, 8);
}

int transaction_write(int fd, TransactionOut t)
{
	uint16_t obj_count;
	uint32_t taskno, length;

	if(!t->t_class) {
		if((taskno = cm_get_taskno(fd)) < 0)
			return -1;

		t->t_taskno = htonl(taskno);
	}

	length = htonl(t->length);
	obj_count = htons(t->obj_count);

	t->iov[4].iov_len = 4;
	t->iov[4].iov_base = (char *)&length;

	t->iov[5].iov_len = 4;
	t->iov[5].iov_base = (char *)&length;

	t->iov[6].iov_len = 2;
	t->iov[6].iov_base = (char *)&obj_count;

#ifdef DEBUG
	debug("--- | OUTGOING TRANSACTION | ---");
	debug("t_class:  %d", ntohs(t->t_class));
	debug("t_id:     %d", ntohs(t->t_id));
	debug("t_taskno: %d", ntohl(t->t_taskno));
	debug("t_error:  %d", ntohl(t->t_error));
	debug("length:   %d", ntohl(length));
	debug("objcnt:   %d", ntohs(obj_count));

	debug("Writing iovec %p of %d entries to %d", t->iov, 7 + t->obj_count, fd);
#endif

	return writev(fd, t->iov, 7 + t->obj_count);
}
