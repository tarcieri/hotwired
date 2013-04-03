#ifndef XMALLOC_H
#define XMALLOC_H

#include <sys/types.h>
#include <stdarg.h>

#ifndef NULL
#define NULL 0
#endif

#define NEW(type)		(type)xmalloc(sizeof(struct _##type))
#define NEWV(type, size)	(type)xmalloc(sizeof(struct _##type) * (size))

void *xmalloc(size_t size);
void *xcalloc(size_t count, size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *str);
char *xvasprintf(const char *format, va_list ap);
char *xasprintf(const char *format, ...);
void xfree(void *ptr);

#endif
