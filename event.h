
#ifndef __EVENT__
#define __EVENT__

#include "list.h"

#define MSG_QUIT			90000
#define MSG_TCP_RCV			90001
#define MSG_UART_RCV		90002

#define MAX_BUFFER_MUM		(4096 + sizeof(int))


typedef int (*fonEvent)(int, int, char *);

struct pool_t {
    struct list_head list;
	int id;
	int len;
	fonEvent callback;
    char *msg;
};

struct msg_t {
	int id;
	int len;
	fonEvent callback;
	char msg[MAX_BUFFER_MUM];
};

void RestoreSigmask(void *param);
void CleanupLock(void *param);

int initEventThread(void);
int sendEvent(int id, int len, char *msg, fonEvent callback = 0);
void *eventThread(void *param);


#endif //__EVENT__