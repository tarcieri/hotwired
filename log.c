#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>

#include <global.h>
#include <log.h>

void log_init()
{
	time_t t;
	struct tm *tm;

	time(&t);
	tm = localtime(&t);

	log("--- | Logfile started | ---");
	log("*** %s %s started on %04d-%02d-%02d", PACKAGE, VERSION, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
}

void log_shutdown()
{
}

void log(char * format, ...)
{
	time_t t;
	va_list ap;
	struct tm tm;
	
	static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

	pthread_mutex_lock(&log_mutex);
	time(&t);
	localtime_r(&t, &tm);

	fprintf(stderr, "[%02d:%02d:%02d] ", tm.tm_hour, tm.tm_min, tm.tm_sec);

	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
	fputc('\n', stderr);
	pthread_mutex_unlock(&log_mutex);
}
