#include <IDM.h>
#include <xmalloc.h>

struct _IDM {
	int start;
	int end;
	struct _IDM *link;
};

IDM idm_create(int base, int size)
{
	IDM ret = NEW(IDM);

	ret->start = base;
	ret->end = base + size;
	ret->link = NULL;

	return ret;
}

void idm_destroy(IDM i)
{
	IDM t;
	
	while(i != NULL) {
		t = i;
		i = i->link;

		xfree(t);
	}
}

int idm_get(IDM *i, int *index)
{
	IDM t;

	if(*i == NULL)
		return -1;

	*index = (*i)->start++;

	if((*i)->start > (*i)->end) {
		t = *i;
		*i = (*i)->link;
		xfree(t);
	}

	return 0;
}

/* This is probably a bit more complicated than it needs to be */
void idm_return(IDM *i, int index)
{
	IDM c, t;

	if(*i == NULL) {
		*i = NEW(IDM);
		(*i)->start = index;
		(*i)->end = index;
		(*i)->link = NULL;

		return;
	} 
	
	if(index < (*i)->start - 1) {
		t = *i;

		*i = NEW(IDM);
		(*i)->start = index;
		(*i)->end = index;
		(*i)->link = t;

		if(t == NULL)
			return;

		c = *i;
	} else if(index == (*i)->start - 1) {
		(*i)->start--;

		return;
	} else if((*i)->link == NULL) {
		if(index > (*i)->end + 1) {
			(*i)->link = NEW(IDM);
			(*i)->link->start = index;
			(*i)->link->end = index;
			(*i)->link->link = NULL;
		} else if(index == (*i)->end + 1)
			(*i)->end++;

		return;
	} else {
		for(c = *i; c->link != NULL && index > c->link->end + 1; c = c->link);

		if(c->link == NULL) {
			c->link = NEW(IDM);
			c->link->start = index;
			c->link->end = index;
			c->link->link = NULL;

			return;
		}
		
		if(index == c->link->end + 1) {
			c = c->link;
			c->end++;
		} else if(index == c->link->start - 1) 
			c->link->start--;
		else if(index < c->link->start - 1 && index > c->end + 1) {
			t = c->link;

			c->link = NEW(IDM);
			c->link->start = index;
			c->link->end = index;
			c->link->link = t;
		}
	}

	if(c->link != NULL && c->end == c->link->start - 1) {
		t = c->link;

		c->end = c->link->end;
		c->link = c->link->link;

		xfree(t);
	}
}
