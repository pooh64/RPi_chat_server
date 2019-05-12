#include "strlist.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/un.h>

#define ECHO_INTERVAL 1
#define SOCKET_PATH "/tmp/echoloop.sock"

char               *echo_server_str;
size_t              echo_server_str_s;
volatile strlist_t *echo_strlist = NULL;
pthread_mutex_t     echo_strlist_mutex = PTHREAD_MUTEX_INITALIZER;


ssize_t writen(int fd, void *buf, size_t size)
{
	char *ptr = buf;
	size_t start_size = size;

	while (size) {
		ssize_t ret = write(fd, ptr, size);
		if (ret == 0)
			return -1;
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		size -= ret;
		ptr += ret;
	}

	return start_size;
}

ssize_t readn(int fd, void *buf, size_t size)
{
	char *ptr = buf;
	size_t start_size = size;

	while (size) {
		ssize_t ret = read(fd, ptr, size);
		if (ret == 0)
			return start_size - size;
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		size -= ret;
		ptr += ret;
	}

	return start_size;
}


void sighandler_echo(int sig)
{
	size_t to_write = echo_server_str_s;
	char *str = echo_server_str;
	if (writen(STDOUT_FILENO, echo_server_str, echo_server_str_s) < 0) {
		perror("Error: write");
		goto handle_err;
	}

	if (echo_strlist == NULL)
		return;

	if (strlist_print(echo_strlist, STDOUT_FILENO) < 0) {
		perror("Error: write");
		goto handle_err;
	}

	return;

handle_err:
	pthread_mutex_lock(&echo_strlist_mutex);
	strlist_delete(echo_strlist);
	exit(EXIT_FAILURE);
}

void ehcoloop_client(int sock, struct sockaddr_un *addr, char *str)
	__attribute__ ((noreturn))
{
	exit(EXIT_SUCCESS);
}

void echoloop_server_worker(int sock)
{
	size_t buf_s;

	if (readn(sock, buf_s, sizeof(buf_s)) != sizeof(buf_s)) {
		fprintf(stderr, "Error: can't get first ack from client\n");
		return;
	}

	char *buf = malloc(buf_s);
	if (!buf) {
		perror("Error: malloc");
		return;
	}

	if (readn(sock, buf, buf_s) != buf_s) {
		fprintf(stderr, "Error: failed to read data from client\n");
		free(buf);
		return;
	}
}

void ehcoloop_server(int serv_sock, struct sockaddr_un *addr)
	__attribute__ ((noreturn))
{
	while (1) {
		int sock = accept(serv_sock, NULL, NULL);
		if (sock < 0) {
			perror("Error: accept");
			exit(EXIT_FAILURE);
		}

	}


	exit(EXIT_SUCCESS);
}


int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Wrong argv\n");
		exit(EXIT_FAILURE);
	}

	/* Server address */
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	addr.sun_path[0] = '\0';
	strncpy(&addr.sun_path[1], SOCKET_PATH, sizeof(addr.sun_path) - 2);

	int sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("Error: socket");
		exit(EXIT_FAILURE);
	}

	if (bind(sock, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
		if (errno == EADDRINUSE)
			echoloop_client(sock, &addr, argv[1]); /* noreturn */
		perror("Error: bind");
		exit(EXIT_FAILURE);
	}

	if (listen(sock, addr) < 0) {
		perror("Error: listen\n");
		exit(EXIT_FAILURE);
	}

	echo_server_str = argv[1];
	ehco_server_str_s = strlen(argv[1]);

	echoloop_server(sock, &addr); /* noreturn */
}
