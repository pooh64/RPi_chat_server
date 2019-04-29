#include "sopbuf.h"
#include <stdlib.h>
#include <sys/sem.h>

struct sopbuf {
	int semid;
	size_t sops_n;
	size_t sops_s;
	struct sembuf *sops;
};

struct sopbuf *sopbuf_new(int semid, size_t size)
{
	struct sopbuf *buf = malloc(sizeof(*buf));
	if (!buf)
		return NULL;

	buf->sops_n = 0;
	buf->semid  = semid;
	buf->sops_s = size;

	buf->sops = malloc(sizeof(*buf->sops) * buf->sops_s);
	if (!buf->sops) {
		free(buf);
		return NULL;
	}

	return buf;
}

void sopbuf_delete(struct sopbuf *buf)
{
	free(buf->sops);
	free(buf);
}

int sopbuf_add(struct sopbuf *buf, unsigned short num, short op, short flg)
{
	if (buf->sops_n == buf->sops_s)
		return -1;

	struct sembuf *to_set = &buf->sops[buf->sops_n++];

	to_set->sem_num = num;
	to_set->sem_op  = op;
	to_set->sem_flg = flg;

	return 0;
}

void sopbuf_clean(struct sopbuf *buf)
{
	buf->sops_n = 0;
}

int sopbuf_flush(struct sopbuf *buf)
{
	if (semop(buf->semid, buf->sops, buf->sops_n) < 0)
		return -1;

	buf->sops_n = 0;
	return 0;
}
