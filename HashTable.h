#ifndef HASHTABLE_H
#define HASHTABLE_H

typedef struct _HashTable *HashTable;

/* Constructor and destructor */

HashTable hash_table_create(void);
/* CreateHashTable returns a handle to a new hash table */

void hash_table_destroy(HashTable table, void (*)(void *));
/* DestroyHashTable destroys the given hash table and calls the given function 
   on the pointer associated with each key.  The given function should free the
   data associated with the keys.  If NULL is given instead, no action is 
   performed on the stored pointers, and only internal structures are freed.
*/

/* Associated operations */

int hash_table_insert(HashTable table, const char *key, void *data);
/* hash_table_insert description:
   Inserts the given zero terminated string key and associated pointer into 
   the table. 
   
   Returns:

    0	-	Entry successfully inserted
   -1	-	Entry already exists
*/

void *hash_table_remove(HashTable table, const char *key);
/* hash_table_remove description:
   Removes the entry associated with the given table and removes the associated 
   pointer.  If no entry is found in the table NULL is returned.
*/

void *hash_table_lookup(HashTable table, const char *key);
/* hash_table_lookup description:
   Returns the pointer associated with the given key, or NULL if not found.
*/

void hash_table_iterate(HashTable table, int (*iterator)(void *data, void *ptr), void *ptr);

int hash_table_size(HashTable table);

#endif
