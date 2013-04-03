/*
   Collection.h - A general purpose data structure
   Copyright (C)2001, 2002 Anthony Arcieri 
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

#ifndef COLLECTION_H
#define COLLECTION_H

/* Data type abstraction */
typedef unsigned long CollectionKey;
typedef struct _Collection *Collection;

/* collection_create:
   Returns a handle to a new collection */
Collection collection_create(void);

/* collection_destroy:
   Destorys the collection.  Calls the given free function on every pointer
   stored in the collection.
 */ 
void collection_destroy(Collection, void (*free_func)(void *));

/* collection_insert:
   Inserts a key and an associated pointer into the given collection.

Returns: 

Entry successfully inserted:	 0 
Entry already exists: 		-1
 */ 
int collection_insert(Collection, CollectionKey, void *);

/* collection_remove:
   Removes the entry associated with the given key from the collection and 
   returns the associated pointer.  If no entry is found in the collection NULL 
   is returned.  */ 
void *collection_remove(Collection, CollectionKey);

/* collection_lookup:
   Returns the pointer associated with the given key, or NULL if not found.
 */
void *collection_lookup(Collection, CollectionKey);

/* collection_iterate description:
   Performs an in-order traversal of the collection, calling the given function
   with the data associated with each key as an argument.  The function should 
   return 1 if continued iteration is desired, or 0 to stop iteration early.  
   A pointer to pass to the callback function may also be given.
 */
void collection_iterate(Collection, 
		int (*iterator)(CollectionKey, void *data, void *ptr), 
		void *ptr);

/* collection_size:
   Returns the number of items in the collection.
*/
int collection_size(Collection);

#endif
