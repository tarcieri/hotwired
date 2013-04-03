/*
   Collection.h - A general purpose data structure
   Copyright (C)2001, 2002 Matthew Bishop, Anthony Arcieri 
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.

 * The name of Anthony Arcieri may not be used to endorse or promote 
 products derived from this software without specific prior written 
 permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT 
 OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED 
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <limits.h>
#include <string.h>
#include <stdlib.h>

#include <Collection.h>
#include <xmalloc.h>

typedef struct _HashNode {
	CollectionKey key;
	void *data;
	struct _HashNode *next;
} *HashNode;

struct _Collection {
	int n_nodes, n_buckets;
	struct _HashNode **buckets;
};

static int primes[] = { 19, 31, 43, 73, 103, 193, 229, 349, 463, 523, 811, 1153,
	1699, 2689, 3673, 5653, 8539, 12843, 19149, 28753, 43051, 64663, 97003,
	145513, 21803, 327211, 490969, 736363, 1104559, 1656829, 2485123, 
	INT_MAX
};

Collection collection_create(void)
{
	int i;
	Collection ret = NEW(Collection);

	ret->n_nodes = 0;
	ret->n_buckets = primes[0];
	ret->buckets = NEWV(HashNode *, primes[0]);

	for(i = 0; i < primes[0]; i++) 
		ret->buckets[i] = NULL;

	return ret;
}

void collection_destroy(Collection table, void (*free_func)(void *))
{
	int i;
	HashNode node, next;

	for(i = 0; i < table->n_buckets; i++) {
		while((node = table->buckets[i]) != NULL) {
			next = node->next;
			if(free_func != NULL) 
				free_func(node->data);
			xfree(node);
			table->buckets[i] = next;
		}
	}

	xfree(table->buckets);
	xfree(table);
}

static int next_prime(int p)
{
	int i = 0;

	while(primes[i] < p)
		i++;

	return primes[i];
}

static void hash_table_rebuild(Collection table, int bucket_count)
{
	int i;
	unsigned j;
	HashNode n, t, *new_buckets = NEWV(HashNode *, bucket_count);

	for(i = 0; i < bucket_count; i++)
		new_buckets[i] = NULL;

	for(i = 0; i < table->n_buckets; i++) {
		n = table->buckets[i];

		while(n) {
			j = n->key % bucket_count;
			t = n->next;

			n->next = new_buckets[j];
			new_buckets[j] = n;
			n = table->buckets[i] = t;
		}
	}

	table->n_buckets = bucket_count;
	xfree(table->buckets);
	table->buckets = new_buckets;
}

static void hash_table_grow(Collection table)
{
	if((float)table->n_nodes / (float)table->n_buckets > 3.0f)
		hash_table_rebuild(table, next_prime(table->n_buckets + 1));
}

static void hash_table_shrink(Collection table)
{
	if((float)table->n_nodes / (float)table->n_buckets > 1.0f)
		hash_table_rebuild(table, next_prime(table->n_buckets / 2));
}

int collection_insert(Collection table, CollectionKey key, void *data)
{
	unsigned i;
	HashNode n, node = NEW(HashNode);

	hash_table_grow(table);

	node->key = key;
	node->data = data;

	i = key % table->n_buckets;
	n = table->buckets[i];

	while(n) {
		if(key == n->key) {
			xfree(node);
			return -1;
		}

		n = n->next;
	}

	node->next = table->buckets[i];
	table->buckets[i] = node;
	table->n_nodes++;

	return 0;
}

void *collection_remove(Collection table, CollectionKey key)
{
	unsigned i;
	HashNode c, t;
	void *ret;

	hash_table_shrink(table);

	i = key % table->n_buckets;

	if(!table->buckets[i] )
		return NULL;

	if(key == table->buckets[i]->key) {
		t = table->buckets[i];
		table->buckets[i] = table->buckets[i]->next;
	} else {
		for(c = table->buckets[i]; c->next != NULL && 
				key != c->next->key; c = c->next);

		if(!c->next)
			return NULL;

		t = c->next;
		c->next = c->next->next;
	}

	ret = t->data;
	xfree(t);

	table->n_nodes--;

	return ret;
}

void *collection_lookup(Collection table, CollectionKey key)
{
	unsigned i = key % table->n_buckets;
	HashNode n = NULL, o = table->buckets[i];

	while(o) {
		if(key == o->key) {
			if(o != table->buckets[i]) {
				n->next = o->next;
				o->next = table->buckets[i];
				table->buckets[i] = o;
			}

			break;
		}

		n = o;
		o = o->next;
	}

	if(!o)
		return NULL;

	return o->data;
}

void collection_iterate(Collection table, int (*iterator)(CollectionKey, void *, void *), void *ptr)
{
	int i;
	HashNode c;

	for(i = 0; i < table->n_buckets; i++) {
		for(c = table->buckets[i]; c != NULL; c = c->next) {
			if(!iterator(c->key, c->data, ptr))
				return;
		}
	}
}

int collection_size(Collection table)
{
	return table->n_nodes;
}
