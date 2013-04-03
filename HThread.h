#ifndef HTHREAD_H
#define HTHREAD_H

typedef struct _HThread *HThread;

#include <global.h>
#include <MQueue.h>

HThread hthread_create();
void hthread_destroy(HThread t);
MQueue hthread_queue(HThread t);

#endif
