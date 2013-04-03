/* util.c - miscellaneous functions
   Most of these are reentrant versions of other functions, provided here for
   the sake of easing portability between the various reentrant implementations.
 */   

#include <global.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

#include <ConnectionManager.h>
#include <machdep.h>
#include <xmalloc.h>

char *xinet_ntoa(uint32_t ip)
{
	uint8_t *c = (uint8_t *)&ip;
	char *ret = (char *)xmalloc(16);

	sprintf(ret, "%d.%d.%d.%d", c[0], c[1], c[2], c[3]);

	return ret;
}

char *conn_ntoa(int fd)
{
	uint32_t ip;

	if(cm_getval(fd, CONN_IP, &ip) < 0)
		return NULL;

	return xinet_ntoa(ip);
}

struct hostent *xgethostbyname(const char *name)
{
	struct hostent *ret, *t;
	static pthread_mutex_t gethostbyname_mutex = PTHREAD_MUTEX_INITIALIZER;

	pthread_mutex_lock(&gethostbyname_mutex);
	if((t = gethostbyname(name)) == NULL) {
		pthread_mutex_unlock(&gethostbyname_mutex);
		return NULL;
	}

	ret = (struct hostent *)xmalloc(sizeof(struct hostent));
	memcpy(ret, t, sizeof(struct hostent));
	pthread_mutex_unlock(&gethostbyname_mutex);

	return ret;
}

void mcpy_int16(uint8_t *ptr, int16_t value)
{
#ifdef HOST_BIGENDIAN
	memcpy(ptr, &value, 2);
#else
	int16_t v = htons(value);
	memcpy(ptr, &v, 2);
#endif
}

void mcpy_int32(uint8_t *ptr, int32_t value)
{
#ifdef HOST_BIGENDIAN
	memcpy(ptr, &value, 4);
#else
	int32_t v = htonl(value);
	memcpy(ptr, &v, 4);
#endif
}

