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
#include <sys/mount.h>    // for mount
#include <fcntl.h>
#include <signal.h>
#include <netinet/tcp.h>
#include <inttypes.h>
#include <sched.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <termios.h>
#include <poll.h>
#include <linux/uinput.h>
#include <linux/input-event-codes.h>

#include <assert.h>
#include <pthread.h>

#include "event.h"
#include "uart.h"

static int thread_running = 0;

static void onChar(int fd, char ch) {
	return;
}

static int onEvent(int id, int len, char *msg) {
	int fd;
	int length;
	char buffer[1028] = {0};
	int i;

	memcpy((void *)&fd, (void *)msg, sizeof(int));
	length = len - sizeof(int);
	memcpy(buffer, (void *)(msg + sizeof(int)), length);
	for (i = 0; i < length; i++)
		onChar(fd, buffer[i]);
	return i;
}

void tell_uart_thread_quit(void) {
	thread_running = 0;
	return;
}

void *uart_thread(void *param) {
	struct termios term = {0}, save = {0};
    int i, res, v, maxfd = 0;
    fd_set rset;
	char line[1028] = {0};
	int ttyfd = -1;
	struct timeval tv = {0};

	assert(0 != param);
	memcpy(&ttyfd, param, sizeof(int));

	assert (tcgetattr(ttyfd, &term) >= 0);
    memcpy((void *)&save, (void *)&term, sizeof(struct termios));
    term.c_cflag |= B115200, term.c_cflag |= CLOCAL, term.c_cflag |= CREAD, term.c_cflag &= ~PARENB, term.c_cflag &= ~CSTOPB;
    term.c_cflag &= ~CSIZE, term.c_cflag |= CS8, term.c_iflag = IGNPAR, term.c_cc[VMIN] = 1, term.c_cc[VTIME] = 0;
    term.c_iflag = 0, term.c_oflag = 0, term.c_lflag = 0;
    cfsetispeed(&term, B115200), cfsetospeed(&term, B115200);

	tcsetattr(ttyfd, TCSANOW, &term);
	v = fcntl(ttyfd, F_GETFL, 0);
	fcntl(ttyfd, F_SETFL, v | O_NONBLOCK);
	tcflush(ttyfd, TCIFLUSH);

	thread_running = 1;
    while(thread_running) {
        FD_ZERO(&rset);
        FD_SET(ttyfd, &rset);
        maxfd = 0;
        if (maxfd < ttyfd)
			maxfd = ttyfd;

		tv.tv_sec = 1;
		res = select(maxfd + 1, &rset, 0, 0, &tv);
		assert(res >= 0);
		if (0 == res)
			continue;
        if (FD_ISSET(ttyfd, &rset)) {
			memcpy((void *)line, (void *)&ttyfd, sizeof(int));
            res = read(ttyfd, (void *)(line + sizeof(int)), sizeof(line) - sizeof(int));
            if (0 >= res)
				break;
            for (i = 0; i < res; i++) {
				send_event_msg(MSG_UART_RCV, res + sizeof(int), line, onEvent);
            }
        }
	}

    tcsetattr(ttyfd, TCSANOW, &save);
    close(ttyfd), ttyfd = -1;
	return 0;
}