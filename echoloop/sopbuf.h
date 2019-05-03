#ifndef SOPBUF_H_
#define SOPBUF_H
#include <stddef.h>

/* A simple interface to handle SystemV semaphores */

typedef struct sopbuf sopbuf_t;

sopbuf_t *sopbuf_new(int semid, size_t size);
void	  sopbuf_delete(sopbuf_t *buf);

int sopbuf_add(sopbuf_t *buf, unsigned short num, short op, short flg);
int sopbuf_semop(sopbuf_t *buf);

#endif /* SOPBUF_H_ */
