#ifndef PASSWORD_H
#define PASSWORD_H

char *password_encrypt(char *key);
int password_check(char *cyphertext, char *plaintext);

#endif
