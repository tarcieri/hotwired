#ifndef STACK_H
#define STACK_H

/* Data type abstraction */
typedef struct _Stack *Stack;

/* Constructor and destructor */
Stack stack_create(void);
void stack_destroy(Stack);

/* Associated operations */
void stack_push(Stack, void *data);
void *stack_pop(Stack);
void *stack_peek(Stack);
int stack_size(Stack);

#endif
