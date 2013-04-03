/*
   Stack.c: A simple stack implementation
*/

#include <global.h>
#include <Stack.h>
#include <xmalloc.h>

typedef struct _StackNode {
	void *data;
	struct _StackNode *l;
} *StackNode;

struct _Stack {
	struct _StackNode *root;
	unsigned int count;
};

/* Stack implementation */
Stack stack_create(void)
{
	Stack ret = NEW(Stack);

	ret->root = NULL;
	ret->count = 0;

	return ret;
}

void stack_destroy(Stack stack)
{
	StackNode t;

	while(stack->root != NULL) {
		t = stack->root;
		stack->root = stack->root->l;

		xfree(t);
	}

	xfree(stack);
}

void stack_push(Stack stack, void *data)
{
	StackNode t = NEW(StackNode);

	t->l = stack->root;
	stack->root = t;
	t->data = data;
	
	stack->count++;
}

void *stack_pop(Stack stack)
{
	StackNode t;
	void *ret;

	if((t = stack->root) == NULL)
		return NULL;

	stack->root = stack->root->l;
	ret = t->data;
	xfree(t);

	stack->count--;

	return ret;
}

void *stack_peek(Stack stack)
{
	if(stack->root == NULL)
		return NULL;

	return stack->root->data;
}

int stack_size(Stack stack)
{
	return stack->count;
}
