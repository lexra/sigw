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

///////////////////////////////////////////////////////////
static void RestoreSigmask(void *param) {
    pthread_sigmask(SIG_SETMASK, (sigset_t *)param, NULL);
}
int main( int argc, char *argv[] ) {
    sigset_t nset, oset;
    struct timespec ts = {0};
    siginfo_t info;
    FILE *f;
    int res = 0, signo = 0, r = 0;
    char path[1024] = {0}, msg[1024] = {0};
    pid_t pid = -1;

///////////////////////////////////////////////////////////
    sigemptyset(&nset), sigaddset(&nset, SIGUSR1), sigaddset(&nset, SIGINT), sigaddset(&nset, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &nset, &oset);
#if 1
    pthread_cleanup_push(RestoreSigmask, (void *)&oset);
#else
    pthread_cleanup_push((void (*routine)(void *))pthread_sigmask, (void *)&oset);
#endif // 1

///////////////////////////////////////////////////////////
    ts.tv_sec = 0, ts.tv_nsec = 1000000 * 300;
    for(;;) {
        memset(&info, 0, sizeof(siginfo_t));
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
        if(SIGUSR1 != signo) {
            printf("(%s %d) UNTRAP SIGNAL=%d\n", __FILE__, __LINE__, signo);
            continue;
        }
        r = info.si_value.sival_int;
        if (0 == r) {
            printf("(%s %d) SIGUSR1=%d\n", __FILE__, __LINE__, info.si_value.sival_int);
            continue;
        }
        r = info.si_value.sival_int;
        sprintf(path, "/tmp/msg-%08X.txt", r);
        f = fopen(path, "r");
        if (0 == f) {
            printf("(%s %d) `%s` NOT FOUND\n", __FILE__, __LINE__, path);
            continue;
        }
        fgets(msg, sizeof(msg), f);
        fclose(f), f = 0;
        unlink(path);
        printf("(%s %d) %s\n", __FILE__, __LINE__, msg);
        if (0 == strcmp(msg, "QUIT") || 0 == strcmp(msg, "quit"))
            break;
    }
    pthread_cleanup_pop(1);
    return res;
}
 
