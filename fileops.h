#ifndef FILEOPS_H
#define FILEOPS_H

#include <Transaction.h>

void f_init();
char *f_addpath(char *str);
char *f_type(char *filename);
TransactionOut f_list_create(TransactionIn t, char *path);
int f_rmdir_recursive(char *path);
	
#endif
