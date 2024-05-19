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

#include "timer.h"


static Timer_t to[MAX_TIMERS] = {
	{ 0, 0, 0, 0}, { 1, 0, 0, 0}, { 2, 0, 0, 0}, { 3, 0, 0, 0}, { 4, 0, 0, 0}, { 5, 0, 0, 0}, { 6, 0, 0, 0}, { 7, 0, 0, 0}, 
	{ 8, 0, 0, 0}, { 9, 0, 0, 0}, {10, 0, 0, 0}, {11, 0, 0, 0}, {12, 0, 0, 0}, {13, 0, 0, 0}, {14, 0, 0, 0}, {15, 0, 0, 0}, 
	{16, 0, 0, 0}, {17, 0, 0, 0}, {18, 0, 0, 0}, {19, 0, 0, 0}, {20, 0, 0, 0}, {21, 0, 0, 0}, {22, 0, 0, 0}, {23, 0, 0, 0}, 
	{24, 0, 0, 0}, {25, 0, 0, 0}, {26, 0, 0, 0}, {27, 0, 0, 0}, {28, 0, 0, 0}, {29, 0, 0, 0}, {30, 0, 0, 0}, {31, 0, 0, 0}, 
	{32, 0, 0, 0}, {33, 0, 0, 0}, {34, 0, 0, 0}, {35, 0, 0, 0}, {36, 0, 0, 0}, {37, 0, 0, 0}, {38, 0, 0, 0}, {39, 0, 0, 0}, 
	{40, 0, 0, 0}, {41, 0, 0, 0}, {42, 0, 0, 0}, {43, 0, 0, 0}, {44, 0, 0, 0}, {45, 0, 0, 0}, {46, 0, 0, 0}, {47, 0, 0, 0}, 
	{48, 0, 0, 0}, {49, 0, 0, 0}, {50, 0, 0, 0}, {51, 0, 0, 0}, {52, 0, 0, 0}, {53, 0, 0, 0}, {54, 0, 0, 0}, {55, 0, 0, 0}, 
	{56, 0, 0, 0}, {57, 0, 0, 0}, {58, 0, 0, 0}, {59, 0, 0, 0}, {60, 0, 0, 0}, {61, 0, 0, 0}, {62, 0, 0, 0}, {63, 0, 0, 0}, 
};

static void onTimer(UINT nId) {
	return;
}

void pollTimer(void) {
	int i;
	struct timespec request = {0};
	unsigned int now;
	UINT id = 0;
	TIMER_FUNC cb;

	//printf("(%s %d) PollTimer()\n", __FILE__, __LINE__);

	for (i = 0; i < MAX_TIMERS; i++) {
		if(0 == to[i].enable)
			continue;
		if(0 == to[i].timeout)
			continue;
AGAIN:
		if (0 != clock_gettime(CLOCK_REALTIME, &request)) {
			sched_yield();
			goto AGAIN;
		}
		now = request.tv_sec * 1000 + request.tv_nsec / 1000000;
		if(now >= to[i].timeout) {
			//id = to[i].id;
			id = to[i].id = i;
			cb = to[i].cb;

			to[i].enable = 0;
			to[i].timeout = 0;
			//to[i].id = 0;
			to[i].id = i;
			to[i].cb = 0;

			if (0 == cb) {
				onTimer(id);
				continue;
			}
			cb(id);
		}
	}

	return;
}

UINT setTimer(UINT nId, UINT nElapse, TIMER_FUNC cb) {
	int i = nId;
	struct timespec request = {0};
	unsigned int et = 0;

	if (nId >= MAX_TIMERS)
		return 0;
AGAIN:
	if (0 != clock_gettime(CLOCK_REALTIME, &request)) {
		sched_yield();
		goto AGAIN;
	}
	et = request.tv_sec * 1000 + request.tv_nsec / 1000000 + nElapse;
	to[i].id = nId;
	to[i].cb = cb;
	to[i].enable = 1;
	to[i].timeout = et;
	return (i + 1);
}

BOOL killTimer(UINT nId) {
	int i = nId;

	if (nId >= MAX_TIMERS)
		return 0;
	to[i].id = 0;
	to[i].cb = 0;
	to[i].enable = 0;
	to[i].timeout = 0;
	return (i + 1);
}
