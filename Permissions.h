#ifndef PERMISSIONS_H
#define PERMISSIONS_H

#include <global.h>

typedef struct _Permissions {
	uint8_t permissions[8];
} *Permissions;

Permissions permissions_create_from_ascii(char *);
Permissions permissions_create_from_binary(void *);
Permissions permissions_copy(Permissions p);
void permissions_destroy(Permissions);

char *permissions_to_ascii(Permissions);
int permissions_check_field(Permissions, int field);
void permissions_set_field(Permissions, int field);
void permissions_unset_field(Permissions, int field);
int permissions_is_superuser(Permissions);
int permissions_is_operator(Permissions);
int permissions_is_visitor(Permissions);

#endif
