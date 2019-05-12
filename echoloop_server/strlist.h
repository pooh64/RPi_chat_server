#ifndef STRLIST_H_
#define STRLIST_H_

#include <stddef.h>

/* A simple sting list to use in echoloop_main */

typedef struct strlist strlist_t;

strlist_t *strlist_new();
void strlist_delete(strlist_t *list);
int strlist_append(strlist_t *list, char *str, size_t str_s);
int strlist_print(strlist_t *list, int fd);

#endif /* STRLIST_H_ */
