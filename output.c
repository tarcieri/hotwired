#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include <output.h>

void debug(char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
	fputc('\n', stderr);
}

void fatal(char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
	fputc('\n', stderr);

	exit(EXIT_FAILURE);
}
