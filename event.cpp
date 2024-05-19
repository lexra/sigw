#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#include <errno.h>
#include <assert.h>

#include <pthread.h>
#include "event.h"

#define MAX_MSG_MUM			256

static struct pool_t pl;
static pthread_cond_t cPoolEmpty;
static int poolEmpty = 1;
static pthread_mutex_t mPoolEmpty = PTHREAD_MUTEX_INITIALIZER;

void RestoreSigmask(void *param) {
    pthread_sigmask(SIG_SETMASK, (sigset_t *)param, NULL);
}

void CleanupLock(void *param) {
    pthread_mutex_unlock((pthread_mutex_t *)param);
}

static int onEvent(int id, int len, char *msg) {
	printf("(%s %d) onEvent()\n", __FILE__, __LINE__);


	return 0;
}

void *eventThread(void *param) {
    struct pool_t *e;
    struct list_head *pos, *q;
    int n = 0, i = 0;
	int res;
	struct msg_t msgs[256] ={0};

	//pthread_sigmask(SIG_BLOCK, (sigset_t *)param, 0);
	for (;;) {
		pthread_mutex_lock(&mPoolEmpty);
		pthread_cleanup_push(CleanupLock, (void *)&mPoolEmpty);
		while (poolEmpty)
			pthread_cond_wait(&cPoolEmpty, &mPoolEmpty);
		pthread_cleanup_pop(1);

		pthread_mutex_lock(&mPoolEmpty); pthread_cleanup_push(CleanupLock, (void *)&mPoolEmpty);
		n = 0;
		list_for_each_safe(pos, q, &pl.list) {
			e = list_entry(pos, struct pool_t, list);
			msgs[n].id = e->id;
			msgs[n].len = e->len;
			msgs[n].callback = e->callback;
			if (e->len > 0)
				memcpy(&msgs[n].msg, e->msg, e->len);
			list_del(pos);
			free(e);
			n++;
		}
		poolEmpty = 1;
		pthread_cleanup_pop(1);

		for (i = 0; i < n; i++) {
			if (MSG_QUIT == msgs[i].id)
				goto BAITOUT;
			if (0 == msgs[i].callback) {
				res = onEvent(msgs[i].id, msgs[i].len, msgs[i].msg);
				continue;
			}
			msgs[i].callback(msgs[i].id, msgs[i].len, msgs[i].msg);
		}
	}

BAITOUT:
	printf("(%s %d) event_thread() return\n", __FILE__, __LINE__);
	return 0;
}

int initEventThread(void) {
	INIT_LIST_HEAD(&pl.list);
	return pthread_cond_init(&cPoolEmpty, NULL);;
}

int sendEvent(int id, int len, char *msg, fonEvent callback) {
	struct pool_t *e;
	int length;

	length = sizeof(struct pool_t) + len + 32;
	e = (struct pool_t *)malloc(length);
	memset((void *)e, 0, length);
	e->id = id;
	e->len = len;
	e->callback = callback;
	e->msg = (char *)e;
	e->msg += sizeof(struct pool_t);
	if (len > 0)
		memcpy(e->msg, msg, len);

	pthread_mutex_lock(&mPoolEmpty); pthread_cleanup_push(CleanupLock, (void *)&mPoolEmpty);
	list_add_tail(&e->list, &pl.list);
	poolEmpty = 0; pthread_cond_signal(&cPoolEmpty);
	pthread_cleanup_pop(1);
	return 0;
}

