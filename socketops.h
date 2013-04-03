#ifndef SOCKETOPS_H
#define SOCKETOPS_H

#include <global.h>

ssize_t read_timeout(int fd, void *buf, size_t nbytes);
ssize_t write_timeout(int fd, void *buf, size_t nbytes);

int read_all(int fd, void *buf, size_t nbytes);
int write_all(int fd, void *buf, size_t nbytes);

int read_int16(int fd, uint16_t *value);
int read_int32(int fd, uint32_t *value);

#endif
