#include <machdep.h>
#ifdef _GNU_SOURCE
#define _XOPEN_SOURCE
#endif

#ifndef _GNU_SOURCE
#define _XOPEN_VERSION 4
#endif

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifndef _GNU_SOURCE
#define _XOPEN_SOURCE
#endif

#include <unistd.h>

#include <xmalloc.h>

static pthread_mutex_t crypt_mutex = PTHREAD_MUTEX_INITIALIZER;

char salt_chars[64] = { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789./" };

char *password_encrypt(char *key)
{
	char *ret, salt[3];
	
	pthread_mutex_lock(&crypt_mutex);
	salt[0] = salt_chars[rand() % 64];
	salt[1] = salt_chars[rand() % 64];
	if((ret = crypt(key, salt)) != NULL)
		ret = xstrdup(ret);
	pthread_mutex_unlock(&crypt_mutex);
	
	return ret;
}

int password_check(char *cyphertext, char *plaintext)
{
	int ret;
	char *pw, salt[3];
	
	strncpy(salt, cyphertext, 2);
	pthread_mutex_lock(&crypt_mutex);
	pw = crypt(plaintext, salt);
	ret = !strcmp(pw, cyphertext);
	pthread_mutex_unlock(&crypt_mutex);

	return ret;
}
