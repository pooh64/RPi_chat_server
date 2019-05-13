#include "strlist.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/un.h>

#include <pthread.h>
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ECHO_INTERVAL 1
#define SOCKET_PATH "/tmp/echoloop.sock"
#define SERVER_MAX_LISTEN 256

char               *echo_server_str;
size_t              echo_server_str_s;
strlist_t *volatile echo_strlist = NULL;
pthread_mutex_t     echo_strlist_mutex = PTHREAD_MUTEX_INITIALIZER;


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


__attribute__ ((noreturn))
void echoloop_client(int sock, struct sockaddr_un *addr, char *str)
{
	if (connect(sock, (struct sockaddr*) addr, sizeof(*addr)) < 0) {
		perror("Error: connect");
		exit(EXIT_FAILURE);
	}

	size_t str_s = strlen(str);
	if (writen(sock, &str_s, sizeof(str_s)) != sizeof(str_s)) {
		fprintf(stderr, "Error: can't send ack to server\n");
		exit(EXIT_FAILURE);
	}

	if (writen(sock, str, str_s) != str_s) {
		fprintf(stderr, "Error: can't send str to server\n");
		exit(EXIT_FAILURE);
	}

	size_t ack;
	if (readn(sock, &ack, sizeof(ack)) != sizeof(ack)) {
		fprintf(stderr, "Error: can't receive ack from server\n");
		exit(EXIT_FAILURE);
	}
	if (ack != str_s) {
		fprintf(stderr, "Error: wrong ack\n");
		exit(EXIT_FAILURE);
	}

	close(sock);

	printf("echoloop for \"%s\" finished!\n", str);
	exit(EXIT_SUCCESS);
}

void sighandler_echo(int sig)
{
	if (writen(STDOUT_FILENO, echo_server_str, echo_server_str_s) < 0) {
		perror("Error: write");
		goto handle_err;
	}
	if (write(STDOUT_FILENO, "\n", 1) != 1) {
		perror("Error: write");
		goto handle_err;
	}

	if (echo_strlist && strlist_print(echo_strlist, STDOUT_FILENO) < 0) {
		perror("Error: write");
		goto handle_err;
	}

	return;

handle_err:
	pthread_mutex_lock(&echo_strlist_mutex);
	if (echo_strlist)
		strlist_delete(echo_strlist);
	exit(EXIT_FAILURE);
}

int prepare_echo()
{
	struct sigaction echo_sa = {
		.sa_handler = sighandler_echo,
		.sa_flags   = SA_RESTART
	};

	struct itimerval echo_time = {
		.it_interval = { .tv_sec = ECHO_INTERVAL, .tv_usec = 0 },
		.it_value    = { .tv_sec = ECHO_INTERVAL, .tv_usec = 0 }
	};

	if (sigaction(SIGALRM, &echo_sa, NULL) < 0) {
		perror("Error: sigaction");
		return -1;
	}

	if (setitimer(ITIMER_REAL, &echo_time, NULL) < 0) {
		perror("Error: setitimer\n");
		return -1;
	}

	return 0;
}

void* echoloop_server_worker(void *arg)
{
	int sock = (intptr_t) arg;

	size_t buf_s;
	if (readn(sock, &buf_s, sizeof(buf_s)) != sizeof(buf_s)) {
		fprintf(stderr, "Error: can't get ack from client\n");
		return NULL;
	}

	char *buf = malloc(buf_s);
	if (!buf) {
		perror("Error: malloc");
		return NULL;
	}

	if (readn(sock, buf, buf_s) != buf_s) {
		fprintf(stderr, "Error: failed to read data from client\n");
		free(buf);
		return NULL;
	}

	if (writen(sock, &buf_s, sizeof(buf_s)) != sizeof(buf_s)) {
		fprintf(stderr, "Error: can't send ack to client\n");
		free(buf);
		return NULL;
	}

	close(sock);

	pthread_mutex_lock(&echo_strlist_mutex);
	if (strlist_append(echo_strlist, buf, buf_s) < 0) {
		pthread_mutex_unlock(&echo_strlist_mutex);
		free(buf);
		perror("Error: malloc");
		return NULL;
	}
	pthread_mutex_unlock(&echo_strlist_mutex);
	return NULL;
}

__attribute__ ((noreturn))
void echoloop_server(int serv_sock)
{
	if (prepare_echo() < 0)
		exit(EXIT_FAILURE);

	while (1) {
		int sock = accept(serv_sock, NULL, NULL);
		if (sock < 0) {
			perror("Error: accept");
			goto handle_err;
		}
		pthread_t worker;
		int ret = pthread_create(&worker, NULL, echoloop_server_worker,
			(void*) (intptr_t) sock);
		if (ret != 0) {
			errno = ret;
			perror("Error: pthread_create");
			goto handle_err;
		}
		pthread_detach(worker);
	}

	close(serv_sock);
	exit(EXIT_SUCCESS);

handle_err:
	close(serv_sock);
	pthread_mutex_lock(&echo_strlist_mutex);
	if (echo_strlist)
		strlist_delete(echo_strlist);
	exit(EXIT_FAILURE);
}


int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Wrong argv\n");
		exit(EXIT_FAILURE);
	}

	/* Ignore sigpipe */
	struct sigaction sa_ignore = {
		.sa_handler = SIG_IGN
	};
	if (sigaction(SIGPIPE, &sa_ignore, NULL) < 0) {
		perror("Error: sigaction");
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

	if (listen(sock, SERVER_MAX_LISTEN) < 0) {
		perror("Error: listen\n");
		exit(EXIT_FAILURE);
	}

	echo_strlist = strlist_new();
	if (!echo_strlist) {
		perror("Error: malloc\n");
		exit(EXIT_FAILURE);
	}
	echo_server_str = argv[1];
	echo_server_str_s = strlen(argv[1]);

	echoloop_server(sock); /* noreturn */
}
