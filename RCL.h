/* Resource Control Locks

   RCLs operate with the following idea in mind: for any shared structure, 
   there exist two types of operations which may be performed, non-destructive 
   operations which make no alterations to the structure (which we will call 
   'read' operations) and destructive operations which do make alterations to 
   the structure (which we will call 'write' operations)

   Since read operations do not alter the structure, it is possible for several
   of these to occur simultaneously, provided that only read operations are
   occuring at a given time.  So, each time we enter a read operation, we
   call rcl_read_lock(), and when we're finished, we call rcl_read_unlock().

   When it comes time to make a destructive change to the structure, we call
   rcl_write_lock().  Immediately upon calling this, all subsequent calls to
   rcl_read_lock() in other threads will block.  rcl_write_lock() will block
   until the last thread performing a read operation has called 
   rcl_read_unlock().  This ensures that there are no read operations occuring
   during our write operation.  After we've finished the write operation,
   we call rcl_write_unlock() which will allow any pending read operations to
   proceed.
*/

#ifndef RCL_H
#define RCL_H

typedef struct _RCL *RCL;

RCL rcl_create(void);
void rcl_destroy(RCL rcl);

/* Shared locks for reading */
void rcl_read_lock(RCL rcl);
void rcl_read_unlock(RCL rcl);

/* Exclusive locks for writing */
void rcl_write_lock(RCL rcl);
void rcl_write_unlock(RCL rcl);

/* For cases where we need to signal a reader to exit, a special two step
   exclusive locking process.  First reserve the lock, then signal the reader.
   The completion step will block until the reader has exited. */
void rcl_write_reserve_lock(RCL rcl);
void rcl_write_complete_lock(RCL rcl);

#endif
