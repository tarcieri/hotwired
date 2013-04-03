#ifndef THREADMANAGER_H
#define THREADMANAGER_H

#include <HThread.h>

int tm_init();
void tm_destroy();

HThread tm_get_thread();
void tm_return_thread(HThread t);
void tm_handle_crash();

#endif
