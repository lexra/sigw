
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <inttypes.h>

#include <assert.h>
#include <pthread.h>

#include "event.h"
#include "tcpsvc.h"

static int thread_running = 0;
static int listen_sd = -1;
static int accept_sd[CONCURRENT_CLIENT_NUMBER];

static int onRecv(int fd, int length, char *buffer) {
	int res = 0;

	//res = write(fd, buffer, length);
	return res;
}

static int onEvent(int id, int len, char *msg) {
	int connfd;
	int length;
	char buffer[MAX_BUFFER_MUM] = {0};

	memcpy((void *)&connfd, (void *)msg, sizeof(int));
	length = len - sizeof(int);
	memcpy(buffer, (void *)(msg + sizeof(int)), length);

	return onRecv(connfd, length, buffer);
}

int tcpsGetConnectionList(int list[]) {
	int i = 0;
	int count = 0;

	for (i = 0; i < CONCURRENT_CLIENT_NUMBER; i++) {
		if (-1 == accept_sd[i])
			continue;
		list[count++] = accept_sd[i];
	}
	return count;
}

void tellTcpsExit(void) {
	thread_running = 0;
	return;
}

void *tcpsThread(void *param) {
	int v = 1;
	int res;
	struct sockaddr_in servaddr ={0};
	fd_set rset = {0};
	struct timeval tv = {0};
	int i;
	int len;

	for (i = 0; i < CONCURRENT_CLIENT_NUMBER; i++)
		accept_sd[i] = -1;

	listen_sd = socket(AF_INET, SOCK_STREAM, 0), assert(-1 != listen_sd);
	res = setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR, (const void *)&v, sizeof(v)), assert(0 == res);
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(LISTEN_PORT);
	res = bind(listen_sd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_in)), assert(0 == res);
	res = listen(listen_sd, 12), assert(0 == res);

	v = fcntl(listen_sd, F_GETFL, 0);
	fcntl(listen_sd, F_SETFL, v | O_NONBLOCK);

	thread_running = 1;
	while (thread_running) {
		int maxfd;
		int clilen, connfd = -1;
		struct sockaddr_in cliaddr;

		FD_ZERO(&rset);

		maxfd = 0;
		if (maxfd < listen_sd)
			maxfd = listen_sd;
		FD_SET(listen_sd, &rset);
		for (i = 0; i < CONCURRENT_CLIENT_NUMBER; i++) {
			if (-1 == accept_sd[i])
				continue;
			if (maxfd < accept_sd[i])
				maxfd = accept_sd[i];
			FD_SET(accept_sd[i], &rset);
		}

		tv.tv_sec = SELECT_EXPIRY_SECONDS;
		res = select(maxfd + 1, &rset, 0, 0, &tv);
		if (0 == res) {
			continue;
		}
		if (0 > res) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
			break;
		}
		if (FD_ISSET(listen_sd, &rset)) {
			int index = -1;

			clilen = sizeof(cliaddr);
			connfd = accept(listen_sd, (struct sockaddr *)&cliaddr, (socklen_t *)&clilen), assert(connfd >= 0);

#if 1
			assert(connfd >= 0);
#else
			if (connfd < 0) {
				printf("(%s %d) connfd < 0\n", __FILE__, __LINE__);
				goto NEXT_ISSET;
			}
#endif // 1

			printf("(%s %d) accept(%d)\n", __FILE__, __LINE__, connfd);
			for (i = 0; i < CONCURRENT_CLIENT_NUMBER; i++) {
				if (-1 != accept_sd[i])
					continue;
				index = i;
				break;
			}
			if (-1 == index) {
				close(connfd);
				goto NEXT_ISSET;
			}
			v = fcntl(connfd, F_GETFL, 0);
			fcntl(connfd, F_SETFL, v | O_NONBLOCK);
			accept_sd[index] = connfd;
			//goto NEXT_ISSET;
			continue;
		}

NEXT_ISSET:
		for (i = 0; i < CONCURRENT_CLIENT_NUMBER; i++) {
			if (-1 == accept_sd[i])
				continue;

			connfd = accept_sd[i];
			if (FD_ISSET(connfd, &rset)) {
				char buffer[MAX_BUFFER_MUM] = {0};

				memcpy((void *)buffer, (void *)&connfd, sizeof(int));

				len = read(connfd, buffer + sizeof(int), MAX_BUFFER_MUM - sizeof(int));
#if 0
				if (len < 0) {
					if (EWOULDBLOCK == errno)
						continue;
					if (EAGAIN == errno)
						continue;
				}
#endif // 0
				assert(len >= 0);
				if (0 == len) {
					close(connfd);
					accept_sd[i] = -1;
					continue;
				}
				sendEvent(MSG_TCP_RCV, len + sizeof(int), buffer, onEvent);
			}
		}
	}

	for (i = 0; i < CONCURRENT_CLIENT_NUMBER; i++) {
		if (-1 != accept_sd[i])
			close(accept_sd[i]), accept_sd[i]= -1;
	}
	close(listen_sd), listen_sd = -1;

	printf("(%s %d) tcpsvc_thread() return\n", __FILE__, __LINE__);
	return 0;
}



