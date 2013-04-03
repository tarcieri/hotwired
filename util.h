#ifndef MISC_H
#define MISC_H

#include <sys/types.h>
#include <inttypes.h>
#include <netdb.h>

char *xinet_ntoa(uint32_t ip);
char *conn_ntoa(int fd);
struct hostent *xgethostbyname(const char *name);
void mcpy_int16(uint8_t *ptr, int16_t value);
void mcpy_int32(uint8_t *ptr, int32_t value);

#endif
