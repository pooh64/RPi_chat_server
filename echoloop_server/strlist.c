#include "strlist.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

struct strelem {
	char *str;
	size_t str_s;
	struct strelem *next;
};

struct strlist {
	struct strelem *first;
	struct strelem *last;
};

struct strlist *strlist_new()
{
	struct strlist *ptr = malloc(sizeof(*ptr));
	if (!ptr)
		return NULL;
	ptr->first = NULL;
	ptr->last = NULL;
	return ptr;
}

void strlist_delete(struct strlist *list)
{
	for (struct strelem *ptr = list->first; ptr != NULL; ptr = ptr->next)
		free(ptr->str);
	free(list);
}

/* All strings here must be dynamically allocated */
/* List may be safely printed if this function is interrupted */
int strlist_append(struct strlist *list, char *str, size_t str_s)
{
	struct strelem *elem = malloc(sizeof(*elem));
	if (!elem)
		return -1;
	elem->str = str;
	elem->str_s = str_s;

	if (!list->first)
		list->first = elem;
	else
		list->last->next = elem;

	list->last = elem;
	return 0;
}

int strlist_print(struct strlist *list, int fd)
{
	for (struct strelem *ptr = list->first; ptr != NULL; ptr = ptr->next) {
		char *str = ptr->str;
		size_t str_s = ptr->str_s;
		while (str_s) {
			ssize_t ret = write(fd, str, str_s);
			if (ret < 0) {
				perror("Error: write");
				return -1;
			}
			str_s -= ret;
		}
		write(fd, "\n", 1);
	}
	return 0;
}
