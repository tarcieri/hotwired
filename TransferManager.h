#ifndef TRANSFER_MANAGER_H
#define TRANSFER_MANAGER_H

#include <global.h>
#include <HThread.h>

void tfm_init();
void tfm_destroy();

int tfm_add_download(int uid, char *filename, int mode, int offset);
int tfm_add_upload(int uid, char *filename, int mode, int offset);
int tfm_register(int tid, int fd, uint32_t ip, HThread t);

int tfm_position(int tid);
int tfm_owner(int tid);

void tfm_remove(int tid);
void tfm_purge_uid(int uid);

#endif
