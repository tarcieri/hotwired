#include <stdlib.h>
#include <string.h>

#include <Permissions.h>
#include <hlid.h>
#include <global.h>
#include <xmalloc.h>

/* XXX I'm sure this could be done better, but I'm lazy */
Permissions permissions_create_from_ascii(char *permbuf)
{
	Permissions p = NEW(Permissions);
	int i;
	char cb[3];
	
	if(permbuf == NULL || strlen(permbuf) != 16) {
		memset(p->permissions, 0, 8);
		return p;
	}

	cb[2] = '\0';

	for(i = 0; i < 8; i++) {
		cb[0] = permbuf[2 * i];
		cb[1] = permbuf[2 * i + 1];
		p->permissions[i] = (uint8_t)strtol(cb, NULL, 16);
	}

	return p;
}

Permissions permissions_create_from_binary(void *permbuf)
{
	Permissions p = NEW(Permissions);

	memcpy(p->permissions, permbuf, 8);
	
	return p;
}

Permissions permissions_copy(Permissions p)
{
	Permissions ret = NEW(Permissions);

	memcpy(ret->permissions, p->permissions, 8);
	return ret;
}

/* XXX Likewise here... */
char *permissions_to_ascii(Permissions p)
{
	int i;
	char *hexbuf = "0123456789ABCDEF", *ret = (char *)xmalloc(17);

	for(i = 0; i < 8; i++) {
		ret[i * 2] = hexbuf[p->permissions[i] >> 4 & 0xF];
		ret[i * 2 + 1] = hexbuf[p->permissions[i] & 0xF];
	}

	ret[16] = '\0';

	return ret;
}

int permissions_check_field(Permissions p, int field)
{
	return (*(p->permissions + (field >> 3))) >> (7 - (field & 7)) & 1;
}

void permissions_set_field(Permissions p, int field)
{
	*(p->permissions + (field >> 3)) |= 1 << (7 - (field & 7));
}

void permissions_unset_field(Permissions p, int field)
{
	*(p->permissions + (field >> 3)) &= ~(1 << (7 - (field & 7)));
}

int permissions_is_superuser(Permissions p)
{
	const uint8_t permbuf[8] = { 255, 243, 207, 255, 255, 128, 0, 0 };

	if(memcmp(p->permissions, permbuf, 8))
		return 0;

	return 1;
}

int permissions_is_operator(Permissions p)
{
	return permissions_check_field(p, HL_PERM_DISCONNECT_USERS);
}

int permissions_is_visitor(Permissions p)
{
	int i;

	for(i = 0; i < 8; i++)
		if(p->permissions[i] != 0)
			return 0;

	return 1;
}

void permissions_destroy(Permissions p)
{
	xfree(p);
}
