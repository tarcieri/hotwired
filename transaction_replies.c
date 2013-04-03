#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <Transaction.h>
#include <hlid.h>
#include <xmalloc.h>

void reply_success(int uid, TransactionIn t)
{
	TransactionOut ot;

	ot = transaction_reply_create(t, 0, 0);
	transaction_write(uid, ot);
	transaction_out_destroy(ot);
}

void reply_error(int uid, TransactionIn t, char *format, ...)
{
	TransactionOut ot;
	char *reply_buffer;
	va_list ap;

	va_start(ap, format);
	reply_buffer = xvasprintf(format, ap);
	va_end(ap);

	ot = transaction_reply_create(t, 1, 1);
	transaction_add_string(ot, HL_ERRORMSG, reply_buffer);
	xfree(reply_buffer);
	transaction_write(uid, ot);
	transaction_out_destroy(ot);
}

void message_error(int uid, char *format, ...)
{
	TransactionOut t;
	char *reply_buffer;
	va_list ap;

	va_start(ap, format);
	reply_buffer = xvasprintf(format, ap);
	va_end(ap);

	t = transaction_create(HL_QUIT_MESSAGE, 1);
	transaction_add_string(t, HL_MESSAGE, reply_buffer);
	xfree(reply_buffer);
	transaction_write(uid, t);
	transaction_out_destroy(t);
}
