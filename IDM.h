#ifndef IDM_H
#define IDM_H

typedef struct _IDM *IDM;

IDM idm_create(int base, int size);
void idm_destroy(IDM i);
int idm_get(IDM *i, int *index);
void idm_return(IDM *i, int index);

#endif
