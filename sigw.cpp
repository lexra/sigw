#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <strings.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <regex.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <signal.h>
#include <sys/msg.h>

#include <pthread.h>
#include <assert.h>
#include <regex.h>

#include "event.h"
#include "tcpsvc.h"
#include "uart.h"
#include "timer.h"

///////////////////////////////////////////////////////////
static UINT interval = 10000;

///////////////////////////////////////////////////////////

static void onSIGUSR1 (int id, int len, char *msg) {
	int res;
	int i;
	int n = 0;
	int list[CONCURRENT_CLIENT_NUMBER] = {0};

	/*if (msg[0] == 'N' && msg[1] == 'F' && msg[2] == 'C') {
		printf("(%s %d) NFC\n", __FILE__, __LINE__);

		n = tcpsGetConnectionList(connfd);
		for (i = 0; i < n; i++) {
			printf("(%s %d) connfd[%d]=%d\n", __FILE__, __LINE__, i, connfd[i]);
			res = write(connfd[i], nfc, sizeof(nfc));
		}
		return;
	}
	if (msg[0] == 'V' && msg[1] == 'E' && msg[2] == 'R' && msg[3] == 'I' && msg[4] == 'F' && msg[5] == 'Y') {
		printf("(%s %d) VERIFY\n", __FILE__, __LINE__);

		n = get_connection_list(connfd);
		for (i = 0; i < n; i++) {
			printf("(%s %d) connfd[%d]=%d\n", __FILE__, __LINE__, i, connfd[i]);
			res = write(connfd[i], verify, sizeof(verify));
		}
		return;
	}
	*/

	//		n = tcpsGetConnectionList(list);
	//		if (n > 0)
	//			ipcSendto(n, list, yaml, strlen(yaml));


	printf("(%s %d) msg=%s\n", __FILE__, __LINE__, msg);
	n = tcpsGetConnectionList(list);
	if (n > 0)
		ipcSendto(n, list, msg, len);
	return;
}

static void onTimer(UINT nId) {
	//if (TIMER_ID_POWER_ON == nid)
	//	uartKidPowerOn();
	return;
}

static void onPeriod(UINT nId) {
	int timeout = 0x0a;
	int standby = 0;

	printf("(%s %d) onPeriod()\n", __FILE__, __LINE__);
	uartKidVerify(standby, timeout);

	setTimer(TIMER_PERIOD, interval, onPeriod);
	return;
}

static int setPollTimer(int period) {
	int res;
	struct itimerval tick = {0};

	tick.it_value.tv_sec = period / 1000000, tick.it_value.tv_usec = period % 1000000;
	tick.it_interval.tv_sec = period / 1000000, tick.it_interval.tv_usec = period % 1000000;
	res = setitimer(ITIMER_REAL, &tick, NULL), assert(0 == res);
	return res;
}

int main(int argc, char *argv[] ) {
    sigset_t nset, oset;
    struct timespec ts = {0};
    FILE *f;
    int res = 0, signo = 0, r = 0;
    char path[1024] = {0};
    char msg[1024 * 32] = {0};
    pid_t pid = -1;

	int ttyfd = -1;
	pthread_t tUart = 0;

	pthread_t tEvent = 0;
	pthread_t tTcp = 0;

///////////////////////////////////////////////////////////
	sigemptyset(&nset), sigaddset(&nset, SIGUSR1), sigaddset(&nset, SIGINT), sigaddset(&nset, SIGTERM), sigaddset(&nset, SIGALRM);
	pthread_sigmask(SIG_BLOCK, &nset, &oset);
	pthread_cleanup_push(RestoreSigmask, (void *)&oset);

///////////////////////////////////////////////////////////
	res = initEventThread();
	res = pthread_create(&tEvent, NULL, eventThread, (void *)&nset), assert(0 == res);
	res = pthread_create(&tTcp, NULL, tcpsThread, (void *)&nset), assert(0 == res);
	ttyfd = open(TTY_SERIAL, O_RDWR | O_NOCTTY | O_NDELAY);
	if (-1 != ttyfd) {
		printf("(%s %d) uartThread\n", __FILE__, __LINE__);
		res = pthread_create(&tUart, NULL, uartThread, (void *)&ttyfd), assert(0 == res);
	}

///////////////////////////////////////////////////////////
	setPollTimer(32);

	//printf("(%s %d) TIMER_KID_POWER_ON\n", __FILE__, __LINE__);
	setTimer(TIMER_KID_VERIFY, 1000, sendKidTimer);

	ts.tv_sec = 0, ts.tv_nsec = 1000000 * 300;
	for(;;) {
		siginfo_t info = {0};
		char *p = 0;
		int l = 0;

		signo = sigtimedwait(&nset, &info, &ts);

		if (-1 == signo && errno == EAGAIN)
			continue;
		if (-1 == signo && errno == EINTR)
			continue;

		if (-1 == signo) {
			printf("(%s %d) SIGTIMEDWAIT() fail, errno=%d, EXIT() \n", __FILE__, __LINE__, errno);
			res = errno;
			break;
		}
		if(SIGINT == signo) {
			printf("(%s %d) SIGINT\n", __FILE__, __LINE__);
			break;
		}
		if(SIGTERM == signo) {
			printf("(%s %d) SIGTERM\n", __FILE__, __LINE__);
			break;
		}
		if(SIGALRM == signo) {
			pollTimer();
			continue;
		}

		if(SIGUSR1 != signo) {
			printf("(%s %d) UNTRAP SIGNAL=%d\n", __FILE__, __LINE__, signo);
			break;
		}

		r = info.si_value.sival_int;
		if (0 == r) {
			printf("(%s %d) SIGUSR1=%d\n", __FILE__, __LINE__, info.si_value.sival_int);
			continue;
		}
		r = info.si_value.sival_int;
		sprintf(path, "/tmp/usr-%08X.yaml", r);
		f = fopen(path, "rb");
		if (0 == f) {
			printf("(%s %d) `%s` NOT FOUND\n", __FILE__, __LINE__, path);
			continue;
		}

		memset(p = msg, 0, sizeof(msg));
		while (1) {
			l = fread(p, sizeof(char), 128, f);
			if (0 == l) break;
			p += l;
		}
		fclose(f), f = 0;
		unlink(path);

		if (msg[0] == 'Q' && msg[1] == 'U' && msg[2] == 'I' && msg[3] == 'T')
			break;
		if (msg[0] == 'q' && msg[1] == 'u' && msg[2] == 'i' && msg[3] == 't')
			break;
		//printf("(%s %d) %s\n", __FILE__, __LINE__, msg);
		//onSIGUSR1 (r, l, msg);
		onSIGUSR1 (r, p - msg, msg);
	}

	if (tUart) {
		tellUartThreadExit();
		pthread_join(tUart, NULL), tUart = 0;
		close(ttyfd);
	}
	if (tTcp) {
		tellTcpsExit();
		pthread_join(tTcp, NULL), tTcp = 0;
	}
	if (tEvent) {
		sendEvent(MSG_QUIT, 0, 0, 0);
		pthread_join(tEvent, NULL), tEvent = 0;
	}

	pthread_cleanup_pop(1);
	return res;
}
 
