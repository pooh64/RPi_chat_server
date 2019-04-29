#include "sopbuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

enum echoloop_semnum {
	SEM_RECEIVER,
	SEM_SENDER,
	SEM_TRANSFER,
	SEM_MAX
};

int main()
{
	int ret = mkfifo("/tmp/echoloop.fifo", 0666);
	if (ret < 0 && errno != EEXIST)
		exit(EXIT_FAILURE);

	key_t key = ftok("/tmp/echoloop.fifo", 1);
	if (key < 0)
		exit(EXIT_FAILURE);

	int semid = semget(key, SEM_MAX, IPC_CREAT);
	if (semid < 0)
		exit(EXIT_FAILURE);

	sopbuf_t *buf = sopbuf_new(semid, 10);
	sopbuf_delete(buf);
	return 0;
}
