#include <machdep.h>

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include <xmalloc.h>

/* Used by the portable vasprintf implementation */
#define INITIAL_BUFFER	128

void *xmalloc(size_t size)
{
	void *ret;

	if((ret = malloc(size)) == NULL) {
		perror("malloc");

		exit(EXIT_FAILURE);
	}

	return ret;
}

void *xcalloc(size_t count, size_t size)
{
	void *ret;

	if((ret = calloc(count, size)) == NULL) {
		perror("calloc");

		exit(EXIT_FAILURE);
	}

	return ret;
}

void *xrealloc(void *ptr, size_t size)
{
	void *ret;

	if((ret = realloc(ptr, size)) == NULL) {
		perror("realloc");

		exit(EXIT_FAILURE);
	}

	return ret;
}

char *xstrdup(const char *str)
{
	char *ret;

	if((ret = strdup(str)) == NULL) {
		perror("strdup");

		exit(EXIT_FAILURE);
	}

	return ret;
}

char *xvasprintf(const char *format, va_list ap)
{
	char *ret;

#if HAVE_VASPRINTF
	if(vasprintf(&ret, format, ap) < 0) {
		perror("asprintf");

		exit(EXIT_FAILURE);
	}

	return ret;
#else
	int l;

	ret = (char *)xmalloc(INITIAL_BUFFER);

	if((l = vsnprintf(ret, INITIAL_BUFFER, format, ap)) < INITIAL_BUFFER)  
		return ret;

	l++;
	xfree(ret);
	ret = (char *)xmalloc(l);

	vsnprintf(ret, l, format, ap);

	return ret;
#endif
}

char *xasprintf(const char *format, ...)
{
	char *ret;
	va_list ap;
		
	va_start(ap, format);
	ret = xvasprintf(format, ap);
	va_end(ap);

	return ret;
}

void xfree(void *ptr)
{
	free(ptr);
}
