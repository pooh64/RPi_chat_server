#include "sopbuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/sem.h>

enum echoloop_semnum {
	SEM_SINGLE,
	SEM_MAIN,
	SEM_SENDER,
	SEM_TR_ACTIVE,
	SEM_TR_DONE,
	SEM_MAX
};

#define MAX_SOPS 8

int echoloop_main_ready(sopbuf_t *sops)
{
	/* Do not start while previous transfer is running */
	sopbuf_add(sops, SEM_TR_ACTIVE, 0, 0);
	sopbuf_add(sops, SEM_MAIN,      1, SEM_UNDO);
	if (sopbuf_semop(sops) < 0) {
		perror("Error: semop");
		return -1;
	}
	return 0;
}

int echoloop_main_enter_section(sopbuf_t *sops)
{
	/* Wait for sender */
	/* Up active */
	sopbuf_add(sops, SEM_SENDER,    -1, 0);
	sopbuf_add(sops, SEM_SENDER,     1, 0);
	sopbuf_add(sops, SEM_TR_ACTIVE,  1, SEM_UNDO);
	if (sopbuf_semop(sops) < 0) {
		perror("Error: semop");
		return -1;
	}

	/* Check that sender process is running */
	/* Wait for sender process to be active */
	sopbuf_add(sops, SEM_SENDER,    -1, IPC_NOWAIT);
	sopbuf_add(sops, SEM_SENDER,     1, 0);
	sopbuf_add(sops, SEM_TR_ACTIVE, -2, 0);
	sopbuf_add(sops, SEM_TR_ACTIVE,  2, 0);
	if (sopbuf_semop(sops) < 0) {
		if (errno == EAGAIN)
			fprintf(stderr, "Error: sender process is dead\n");
		else
			perror("Error: semop");
		return -1;
	}
	return 0;
}

int echoloop_main_quit_section(sopbuf_t *sops)
{
	/* Transfer done */
	sopbuf_add(sops, SEM_TR_DONE, 1, SEM_UNDO);
	if (sopbuf_semop(sops) < 0) {
		perror("Error: semop");
		return -1;
	}

	/* Check that sender process is active */
	/* Wait for sender */
	/* Release main lock */
	sopbuf_add(sops, SEM_TR_ACTIVE, -2, IPC_NOWAIT);
	sopbuf_add(sops, SEM_TR_ACTIVE,  2, 0);
	sopbuf_add(sops, SEM_TR_DONE,   -2, 0);
	sopbuf_add(sops, SEM_TR_DONE,    2, 0);
	sopbuf_add(sops, SEM_MAIN,      -1, SEM_UNDO);
	if (sopbuf_semop(sops) == -1) {
		if (errno == EAGAIN)
			fprintf(stderr, "Error: main process failed\n");
		else
			perror("Error: semop");
		return -1;
	}

	/* Success, wait for sender */
	/* Disable transfer */
	/* Restore SEM_TR_DONE */
	sopbuf_add(sops, SEM_SENDER,     0, 0);
	sopbuf_add(sops, SEM_TR_ACTIVE, -1, SEM_UNDO);
	sopbuf_add(sops, SEM_TR_DONE,   -1, SEM_UNDO);
	if (sopbuf_semop(sops) == -1) {
		perror("Error: semop");
		return -1;
	}
	return 0;
}

int echoloop_main_receive(sopbuf_t *sops, int fifo_fd)
{
	if (echoloop_main_ready(sops) < 0)
		return -1;

	if (echoloop_main_enter_section(sops) < 0)
		return -1;

	char buf[512];
	size_t buf_s = 512;
	size_t ret;
	do {
		ret = read(fifo_fd, buf, buf_s);
		if (ret < 0) {
			perror("Error: read");
			return -1;
		}
		if (write(STDOUT_FILENO, buf, ret) != ret) {
			perror("Error: write");
			return -1;
		}
	} while (ret);
	write(STDOUT_FILENO, "\n", 1);

	if (echoloop_main_quit_section(sops) < 0)
		return -1;
	return 0;
}

int echoloop_main(sopbuf_t *sops, char *data)
{
	int fifo_fd = open("/tmp/echoloop.fifo", O_RDONLY | O_NONBLOCK);
	if (fifo_fd < 0) {
		perror("Error: open");
		return -1;
	}
	if (fcntl(fifo_fd, F_SETFL, 0) < 0) {
		perror("Error: fnctl");
		return -1;
	}

	while (1) {
		if (echoloop_main_receive(sops, fifo_fd) < 0) {
			close(fifo_fd);
			return -1;
		}
	}

	close(fifo_fd);

	return 0;
}

int echoloop_sender_capture(sopbuf_t *sops)
{
	/* Capture sender "mutex" */
	/* Do not start while previous transfer is running */
	sopbuf_add(sops, SEM_TR_ACTIVE, 0, 0);
	sopbuf_add(sops, SEM_SENDER,    0, 0);
	sopbuf_add(sops, SEM_SENDER,    1, SEM_UNDO);
	if (sopbuf_semop(sops) < 0) {
		perror("Error: semop");
		return -1;
	}
	return 0;
}

int echoloop_sender_enter_section(sopbuf_t *sops)
{
	/* Check that main process is running */
	/* Wait main process to be redy for transfer */
	/* Up active */
	sopbuf_add(sops, SEM_SINGLE,    -1, IPC_NOWAIT);
	sopbuf_add(sops, SEM_SINGLE,     1, 0);
	sopbuf_add(sops, SEM_MAIN,      -1, 0);
	sopbuf_add(sops, SEM_MAIN,       1, 0);
	sopbuf_add(sops, SEM_TR_ACTIVE,  1, SEM_UNDO);
	if (sopbuf_semop(sops) < 0) {
		if (errno == EAGAIN)
			fprintf(stderr, "Error: main process is dead\n");
		else
			perror("Error: semop");
		return -1;
	}

	/* Check that main process is ready/running */
	/* Wait for main process to be active */
	sopbuf_add(sops, SEM_MAIN,      -1, IPC_NOWAIT);
	sopbuf_add(sops, SEM_MAIN,       1, 0);
	sopbuf_add(sops, SEM_TR_ACTIVE, -2, 0);
	sopbuf_add(sops, SEM_TR_ACTIVE,  2, 0);
	if (sopbuf_semop(sops) < 0) {
		if (errno == EAGAIN)
			fprintf(stderr, "Error: main process is dead\n");
		else
			perror("Error: semop");
		return -1;
	}
	return 0;
}

int echoloop_sender_quit_section(sopbuf_t *sops)
{
	/* Transfer done */
	sopbuf_add(sops, SEM_TR_DONE, 1, SEM_UNDO);
	if (sopbuf_semop(sops) < 0) {
		perror("Error: semop");
		return -1;
	}

	/* Check that main process is active */
	/* Wait for main */
	/* Release sender lock */
	sopbuf_add(sops, SEM_TR_ACTIVE, -2, IPC_NOWAIT);
	sopbuf_add(sops, SEM_TR_ACTIVE,  2, 0);
	sopbuf_add(sops, SEM_TR_DONE,   -2, 0);
	sopbuf_add(sops, SEM_TR_DONE,    2, 0);
	sopbuf_add(sops, SEM_SENDER,    -1, SEM_UNDO);
	if (sopbuf_semop(sops) == -1) {
		if (errno == EAGAIN)
			fprintf(stderr, "Error: main process failed\n");
		else
			perror("Error: semop");
		return -1;
	}

	/* Success, wait for main */
	/* Disable transfer */
	/* Retsore SEM_TR_DONE */
	sopbuf_add(sops, SEM_MAIN,       0, 0);
	sopbuf_add(sops, SEM_TR_ACTIVE, -1, SEM_UNDO);
	sopbuf_add(sops, SEM_TR_DONE,   -1, SEM_UNDO);
	if (sopbuf_semop(sops) == -1) {
		perror("Error: semop");
		return -1;
	}
	return 0;
}

int echoloop_sender(sopbuf_t *sops, char *data)
{
	if (echoloop_sender_capture(sops) < 0)
		return -1;

	int fifo_fd = open("/tmp/echoloop.fifo", O_WRONLY | O_NONBLOCK);
	if (fifo_fd < 0) {
		if (errno == ENXIO)
			fprintf(stderr, "Error: main process failed\n");
		else
			perror("Error: open");
		return -1;
	}
	if (fcntl(fifo_fd, F_SETFL, 0) < 0) {
		perror("Error: fnctl");
		return -1;
	}

	if (echoloop_sender_enter_section(sops) < 0)
		return -1;

	size_t data_s = strlen(data);
	while (data_s) {
		int ret = write(fifo_fd, data, data_s);
		if (ret < 0) {
			perror("Error: write");
			return -1;
		}
		data_s -= ret;
	}
	close(fifo_fd);

	if (echoloop_sender_quit_section(sops) < 0)
		return -1;

	return 0;
}

int echoloop_start(sopbuf_t *sops, char *str)
{
	/* Try to capture single "mutex" */
	/* Do not start while previous transfer is active */
	sopbuf_add(sops, SEM_TR_ACTIVE, 0, 0);
	sopbuf_add(sops, SEM_SINGLE,    0, IPC_NOWAIT);
	sopbuf_add(sops, SEM_SINGLE,    1, SEM_UNDO);
	if (sopbuf_semop(sops) < 0) {
		if (errno != EAGAIN) {
			perror("Error: semop");
			return -1;
		}
		if (echoloop_sender(sops, str) < 0)
			return -1;
	} else {
		if (echoloop_main(sops, str) < 0)
			return -1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Wrong argv\n");
		exit(EXIT_FAILURE);
	}

	int ret = mkfifo("/tmp/echoloop.fifo", 0666);
	if (ret < 0 && errno != EEXIST) {
		perror("Error: mkfifo");
		exit(EXIT_FAILURE);
	}

	key_t key = ftok("/tmp/echoloop.fifo", 1);
	if (key < 0) {
		perror("Error: ftok");
		exit(EXIT_FAILURE);
	}

	int semid = semget(key, SEM_MAX, IPC_CREAT | 0644);
	if (semid < 0) {
		perror("Error: semget");
		exit(EXIT_FAILURE);
	}

	sopbuf_t *sops = sopbuf_new(semid, MAX_SOPS);
	if (!sops) {
		perror("Error: sopbuf_new");
		exit(EXIT_FAILURE);
	}

	if (echoloop_start(sops, argv[1]) < 0) {
		fprintf(stderr, "Error: echoloop failed\n");
		sopbuf_delete(sops);
		exit(EXIT_FAILURE);
	}

	sopbuf_delete(sops);
	return 0;
}
