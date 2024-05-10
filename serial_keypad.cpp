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
#include <pthread.h>

#include "list.h"

struct pool_t {
    struct list_head list;
    unsigned char c;
};
static struct pool_t pl;
static pthread_t tEmit = 0;
static pthread_cond_t cPoolEmpty;
static int poolEmpty = 1;
static pthread_mutex_t mPoolEmpty = PTHREAD_MUTEX_INITIALIZER;

#define TTY_SERIAL                    "/dev/ttyUSB0"
#define SELECT_EXPIRY_NINISECONDS    100ULL
#define ACTION_DOWN                    1
#define ACTION_UP                    0

#define _u1_                        0
#define _d1_                        1
#define _l1_                        2
#define _r1_                         3
#define _s1_                         4
#define _x1_                         5
#define _y1_                         6
#define _z1_                         7
#define _a1_                         8
#define _b1_                         9
#define _c1_                         10
#define _u2_                        11
#define _d2_                        12
#define _l2_                        13
#define _r2_                         14
#define _s2_                         15
#define _x2_                         16
#define _y2_                         17
#define _z2_                         18
#define _a2_                         19
#define _b2_                         20
#define _c2_                         21
#define _m_                         22
#define _w_                         23
#define _U1_                        (1 << _u1_)
#define _D1_                        (1 << _d1_)
#define _L1_                        (1 << _l1_)
#define _R1_                         (1 << _r1_)
#define _S1_                         (1 << _s1_)
#define _X1_                         (1 << _x1_)
#define _Y1_                         (1 << _y1_)
#define _Z1_                         (1 << _z1_)
#define _A1_                         (1 << _a1_)
#define _B1_                         (1 << _b1_)
#define _C1_                         (1 << _c1_)
#define _U2_                        (1 << _u2_)
#define _D2_                        (1 << _d2_)
#define _L2_                        (1 << _l2_)
#define _R2_                         (1 << _r2_)
#define _S2_                         (1 << _s2_)
#define _X2_                         (1 << _x2_)
#define _Y2_                         (1 << _y2_)
#define _Z2_                         (1 << _z2_)
#define _A2_                         (1 << _a2_)
#define _B2_                         (1 << _b2_)
#define _C2_                         (1 << _c2_)
#define _M_                         (1 << _m_)
#define _W_                         (1 << _w_)

struct code_t {
    int index; int code; const char *name;
};
static struct code_t keypad_codes[] = {
    {_u1_, KEY_UP, "P1_UP"},
    {_d1_, KEY_DOWN, "P1_DW"},
    {_l1_, KEY_LEFT, "P1_LF"},
    {_r1_, KEY_RIGHT, "P1_RG"},
    {_s1_, KEY_ENTER, "P1_ST"},
    {_x1_, KEY_X, "P1_X "},
    {_y1_, KEY_Y, "P1_Y "},
    {_z1_, KEY_Z, "P1_Z "},
    {_a1_, KEY_A, "P1_A "},
    {_b1_, KEY_B, "P1_B "},
    {_c1_, KEY_C, "P1_C "},
    {_u2_, KEY_KP8, "P2_UP"},
    {_d2_, KEY_KP2, "P2_DW"},
    {_l2_, KEY_KP4, "P2_LF"},
    {_r2_, KEY_KP6, "P2_RG"},
    {_s2_, KEY_SPACE, "P2_ST"},
    {_x2_, KEY_F4, "P2_X "},
    {_y2_, KEY_F5, "P2_Y "},
    {_z2_, KEY_F6, "P2_Z "},
    {_a2_, KEY_F1, "P2_A "},
    {_b2_, KEY_F8, "P2_B "},
    {_c2_, KEY_F3, "P2_C "},
    {_m_, KEY_M, "MD"},
    {_w_, KEY_R, "RW"},
};
#define KEYPAD_KEYNUMBER    (sizeof(keypad_codes)/sizeof(struct code_t))

static unsigned int uartNowEvents = 0;
static int uinputfd = -1;
static int ttyfd = -1;
static char ttyLine[1024] = {0};
static int ttyLineLength = 0;
#define PRINTF(f_, ...)        \
        sprintf(ttyLine, f_, ##__VA_ARGS__), \
        ttyLineLength = strlen(ttyLine), \
        ttyLine[ttyLineLength] = 0, \
        write(ttyfd, (void *)ttyLine, ttyLineLength + 1)

static void emit(int fd, int type, int code, int val) {
    struct input_event ie = {0};
    ie.type = type; ie.code = code; ie.value = val;
    write(fd, &ie, sizeof(ie));
}

static void CleanupLock(void *param) {
    pthread_mutex_unlock((pthread_mutex_t *)param);
}
static void *EmitThread(void *param) {
    unsigned char ch;
    struct pool_t *e;        // entry
    struct list_head *pos, *q;
    unsigned char buffer[1024 * 4] = {0};
    int n = 0, i = 0;

/////////////////////////////
// Wait for somebody tell
WAIT:
    pthread_mutex_lock(&mPoolEmpty); pthread_cleanup_push(CleanupLock, (void *)&mPoolEmpty);
    while (poolEmpty)            pthread_cond_wait(&cPoolEmpty, &mPoolEmpty);
    pthread_cleanup_pop(1);

/////////////////////////////
// <need-fast>
    pthread_mutex_lock(&mPoolEmpty); pthread_cleanup_push(CleanupLock, (void *)&mPoolEmpty);
    n = 0;
    list_for_each_safe(pos, q, &pl.list) {
        e = list_entry(pos, struct pool_t, list); memcpy(&buffer[n++], &e->c, sizeof(unsigned char)); list_del(pos), free(e);
    }
    poolEmpty = 1;
    pthread_cleanup_pop(1);
// </need-fast>

    for (i = 0; i < n; i++) {
        ch = buffer[i];
        if (ch == (unsigned char)'U')        emit(uinputfd, EV_KEY, keypad_codes[_u1_].code, ACTION_DOWN);
        if (ch == (unsigned char)'u')        emit(uinputfd, EV_KEY, keypad_codes[_u1_].code, ACTION_UP);
        if (ch == (unsigned char)'D')        emit(uinputfd, EV_KEY, keypad_codes[_d1_].code, ACTION_DOWN);
        if (ch == (unsigned char)'d')        emit(uinputfd, EV_KEY, keypad_codes[_d1_].code, ACTION_UP);
        if (ch == (unsigned char)'L')        emit(uinputfd, EV_KEY, keypad_codes[_l1_].code, ACTION_DOWN);
        if (ch == (unsigned char)'l')        emit(uinputfd, EV_KEY, keypad_codes[_l1_].code, ACTION_UP);
        if (ch == (unsigned char)'R')        emit(uinputfd, EV_KEY, keypad_codes[_r1_].code, ACTION_DOWN);
        if (ch == (unsigned char)'r')        emit(uinputfd, EV_KEY, keypad_codes[_r1_].code, ACTION_UP);
        if (ch == (unsigned char)'S')        emit(uinputfd, EV_KEY, keypad_codes[_s1_].code, ACTION_DOWN);
        if (ch == (unsigned char)'s')        emit(uinputfd, EV_KEY, keypad_codes[_s1_].code, ACTION_UP);
        if (ch == (unsigned char)'X')        emit(uinputfd, EV_KEY, keypad_codes[_x1_].code, ACTION_DOWN);
        if (ch == (unsigned char)'x')        emit(uinputfd, EV_KEY, keypad_codes[_x1_].code, ACTION_UP);
        if (ch == (unsigned char)'Y')        emit(uinputfd, EV_KEY, keypad_codes[_y1_].code, ACTION_DOWN);
        if (ch == (unsigned char)'y')        emit(uinputfd, EV_KEY, keypad_codes[_y1_].code, ACTION_UP);
        if (ch == (unsigned char)'Z')        emit(uinputfd, EV_KEY, keypad_codes[_z1_].code, ACTION_DOWN);
        if (ch == (unsigned char)'z')        emit(uinputfd, EV_KEY, keypad_codes[_z1_].code, ACTION_UP);
        if (ch == (unsigned char)'A')        emit(uinputfd, EV_KEY, keypad_codes[_a1_].code, ACTION_DOWN);
        if (ch == (unsigned char)'a')        emit(uinputfd, EV_KEY, keypad_codes[_a1_].code, ACTION_UP);
        if (ch == (unsigned char)'B')        emit(uinputfd, EV_KEY, keypad_codes[_b1_].code, ACTION_DOWN);
        if (ch == (unsigned char)'b')        emit(uinputfd, EV_KEY, keypad_codes[_b1_].code, ACTION_UP);
        if (ch == (unsigned char)'C')        emit(uinputfd, EV_KEY, keypad_codes[_c1_].code, ACTION_DOWN);
        if (ch == (unsigned char)'c')        emit(uinputfd, EV_KEY, keypad_codes[_c1_].code, ACTION_UP);
        if (ch == (unsigned char)'H')        emit(uinputfd, EV_KEY, keypad_codes[_u2_].code, ACTION_DOWN);
        if (ch == (unsigned char)'h')        emit(uinputfd, EV_KEY, keypad_codes[_u2_].code, ACTION_UP);
        if (ch == (unsigned char)'I')        emit(uinputfd, EV_KEY, keypad_codes[_d2_].code, ACTION_DOWN);
        if (ch == (unsigned char)'i')        emit(uinputfd, EV_KEY, keypad_codes[_d2_].code, ACTION_UP);
        if (ch == (unsigned char)'J')        emit(uinputfd, EV_KEY, keypad_codes[_l2_].code, ACTION_DOWN);
        if (ch == (unsigned char)'j')        emit(uinputfd, EV_KEY, keypad_codes[_l2_].code, ACTION_UP);
        if (ch == (unsigned char)'K')        emit(uinputfd, EV_KEY, keypad_codes[_r2_].code, ACTION_DOWN);
        if (ch == (unsigned char)'k')        emit(uinputfd, EV_KEY, keypad_codes[_r2_].code, ACTION_UP);
        if (ch == (unsigned char)'T')        emit(uinputfd, EV_KEY, keypad_codes[_s2_].code, ACTION_DOWN);
        if (ch == (unsigned char)'t')        emit(uinputfd, EV_KEY, keypad_codes[_s2_].code, ACTION_UP);
        if (ch == (unsigned char)'O')        emit(uinputfd, EV_KEY, keypad_codes[_x2_].code, ACTION_DOWN);
        if (ch == (unsigned char)'o')        emit(uinputfd, EV_KEY, keypad_codes[_x2_].code, ACTION_UP);
        if (ch == (unsigned char)'P')        emit(uinputfd, EV_KEY, keypad_codes[_y2_].code, ACTION_DOWN);
        if (ch == (unsigned char)'p')        emit(uinputfd, EV_KEY, keypad_codes[_y2_].code, ACTION_UP);
        if (ch == (unsigned char)'Q')        emit(uinputfd, EV_KEY, keypad_codes[_z2_].code, ACTION_DOWN);
        if (ch == (unsigned char)'q')        emit(uinputfd, EV_KEY, keypad_codes[_z2_].code, ACTION_UP);
        if (ch == (unsigned char)'G')        emit(uinputfd, EV_KEY, keypad_codes[_a2_].code, ACTION_DOWN);
        if (ch == (unsigned char)'g')        emit(uinputfd, EV_KEY, keypad_codes[_a2_].code, ACTION_UP);
        if (ch == (unsigned char)'E')        emit(uinputfd, EV_KEY, keypad_codes[_b2_].code, ACTION_DOWN);
        if (ch == (unsigned char)'e')        emit(uinputfd, EV_KEY, keypad_codes[_b2_].code, ACTION_UP);
        if (ch == (unsigned char)'F')        emit(uinputfd, EV_KEY, keypad_codes[_c2_].code, ACTION_DOWN);
        if (ch == (unsigned char)'f')        emit(uinputfd, EV_KEY, keypad_codes[_c2_].code, ACTION_UP);
        if (ch == (unsigned char)'M')        emit(uinputfd, EV_KEY, keypad_codes[_m_].code, ACTION_DOWN);
        if (ch == (unsigned char)'m')        emit(uinputfd, EV_KEY, keypad_codes[_m_].code, ACTION_UP);
        if (ch == (unsigned char)'W')        emit(uinputfd, EV_KEY, keypad_codes[_w_].code, ACTION_DOWN);
        if (ch == (unsigned char)'w')        emit(uinputfd, EV_KEY, keypad_codes[_w_].code, ACTION_UP);
        emit(uinputfd, EV_SYN, SYN_REPORT, 0);
/////////////////////////////
// 
        usleep(1);
    }
    goto WAIT;
    return 0;
}
static void onUartChar(unsigned char ch) {
    struct pool_t *e;
    if (ch != (unsigned char)'u' && ch != (unsigned char)'U' && 
        ch != (unsigned char)'d' && ch != (unsigned char)'D' && 
        ch != (unsigned char)'l' && ch != (unsigned char)'L' &&
        ch != (unsigned char)'r' && ch != (unsigned char)'R' && 
        ch != (unsigned char)'s' && ch != (unsigned char)'S' && 
        ch != (unsigned char)'x' && ch != (unsigned char)'X' &&
        ch != (unsigned char)'y' && ch != (unsigned char)'Y' && 
        ch != (unsigned char)'z' && ch != (unsigned char)'Z' && 
        ch != (unsigned char)'a' && ch != (unsigned char)'A' &&
        ch != (unsigned char)'b' && ch != (unsigned char)'B' && 
        ch != (unsigned char)'c' && ch != (unsigned char)'C' &&
        ch != (unsigned char)'h' && ch != (unsigned char)'H' && 
        ch != (unsigned char)'i' && ch != (unsigned char)'I' && 
        ch != (unsigned char)'j' && ch != (unsigned char)'J' && 
        ch != (unsigned char)'k' && ch != (unsigned char)'K' && 
        ch != (unsigned char)'t' && ch != (unsigned char)'T' && 
        ch != (unsigned char)'o' && ch != (unsigned char)'O' && 
        ch != (unsigned char)'p' && ch != (unsigned char)'P' && 
        ch != (unsigned char)'q' && ch != (unsigned char)'Q' && 
        ch != (unsigned char)'g' && ch != (unsigned char)'G' && 
        ch != (unsigned char)'e' && ch != (unsigned char)'E' && 
        ch != (unsigned char)'f' && ch != (unsigned char)'F' && 
        ch != (unsigned char)'m' && ch != (unsigned char)'M' && 
        ch != (unsigned char)'w' && ch != (unsigned char)'W'
    )    return;
    if (ch == (unsigned char)'u') { uartNowEvents &= ~_U1_; goto TELL_WAIT; }
    if (ch == (unsigned char)'d') { uartNowEvents &= ~_D1_; goto TELL_WAIT; }
    if (ch == (unsigned char)'l') { uartNowEvents &= ~_L1_; goto TELL_WAIT; }
    if (ch == (unsigned char)'r') { uartNowEvents &= ~_R1_; goto TELL_WAIT; }
    if (ch == (unsigned char)'s') { uartNowEvents &= ~_S1_; goto TELL_WAIT; }
    if (ch == (unsigned char)'x') { uartNowEvents &= ~_X1_; goto TELL_WAIT; }
    if (ch == (unsigned char)'y') { uartNowEvents &= ~_Y1_; goto TELL_WAIT; }
    if (ch == (unsigned char)'z') { uartNowEvents &= ~_Z1_; goto TELL_WAIT; }
    if (ch == (unsigned char)'a') { uartNowEvents &= ~_A1_; goto TELL_WAIT; }
    if (ch == (unsigned char)'b') { uartNowEvents &= ~_B1_; goto TELL_WAIT; }
    if (ch == (unsigned char)'c') { uartNowEvents &= ~_C1_; goto TELL_WAIT; }
    if (ch == (unsigned char)'U') { uartNowEvents |= _U1_; goto TELL_WAIT; }
    if (ch == (unsigned char)'D') { uartNowEvents |= _D1_; goto TELL_WAIT; }
    if (ch == (unsigned char)'L') { uartNowEvents |= _L1_; goto TELL_WAIT; }
    if (ch == (unsigned char)'R') { uartNowEvents |= _R1_; goto TELL_WAIT; }
    if (ch == (unsigned char)'S') { uartNowEvents |= _S1_; goto TELL_WAIT; }
    if (ch == (unsigned char)'X') { uartNowEvents |= _X1_; goto TELL_WAIT; }
    if (ch == (unsigned char)'Y') { uartNowEvents |= _Y1_; goto TELL_WAIT; }
    if (ch == (unsigned char)'Z') { uartNowEvents |= _Z1_; goto TELL_WAIT; }
    if (ch == (unsigned char)'A') { uartNowEvents |= _A1_; goto TELL_WAIT; }
    if (ch == (unsigned char)'B') { uartNowEvents |= _B1_; goto TELL_WAIT; }
    if (ch == (unsigned char)'C') { uartNowEvents |= _C1_; goto TELL_WAIT; }
    if (ch == (unsigned char)'h') { uartNowEvents &= ~_U2_; goto TELL_WAIT; }
    if (ch == (unsigned char)'i') { uartNowEvents &= ~_D2_; goto TELL_WAIT; }
    if (ch == (unsigned char)'j') { uartNowEvents &= ~_L2_; goto TELL_WAIT; }
    if (ch == (unsigned char)'k') { uartNowEvents &= ~_R2_; goto TELL_WAIT; }
    if (ch == (unsigned char)'t') { uartNowEvents &= ~_S2_; goto TELL_WAIT; }
    if (ch == (unsigned char)'o') { uartNowEvents &= ~_X2_; goto TELL_WAIT; }
    if (ch == (unsigned char)'p') { uartNowEvents &= ~_Y2_; goto TELL_WAIT; }
    if (ch == (unsigned char)'q') { uartNowEvents &= ~_Z2_; goto TELL_WAIT; }
    if (ch == (unsigned char)'g') { uartNowEvents &= ~_A2_; goto TELL_WAIT; }
    if (ch == (unsigned char)'e') { uartNowEvents &= ~_B2_; goto TELL_WAIT; }
    if (ch == (unsigned char)'f') { uartNowEvents &= ~_C2_; goto TELL_WAIT; }
    if (ch == (unsigned char)'H') { uartNowEvents |= _U2_; goto TELL_WAIT; }
    if (ch == (unsigned char)'I') { uartNowEvents |= _D2_; goto TELL_WAIT; }
    if (ch == (unsigned char)'J') { uartNowEvents |= _L2_; goto TELL_WAIT; }
    if (ch == (unsigned char)'K') { uartNowEvents |= _R2_; goto TELL_WAIT; }
    if (ch == (unsigned char)'T') { uartNowEvents |= _S2_; goto TELL_WAIT; }
    if (ch == (unsigned char)'O') { uartNowEvents |= _X2_; goto TELL_WAIT; }
    if (ch == (unsigned char)'P') { uartNowEvents |= _Y2_; goto TELL_WAIT; }
    if (ch == (unsigned char)'Q') { uartNowEvents |= _Z2_; goto TELL_WAIT; }
    if (ch == (unsigned char)'G') { uartNowEvents |= _A2_; goto TELL_WAIT; }
    if (ch == (unsigned char)'E') { uartNowEvents |= _B2_; goto TELL_WAIT; }
    if (ch == (unsigned char)'F') { uartNowEvents |= _C2_; goto TELL_WAIT; }
    if (ch == (unsigned char)'m') { uartNowEvents &= ~_M_; goto TELL_WAIT; }
    if (ch == (unsigned char)'w') { uartNowEvents &= ~_W_; goto TELL_WAIT; }
    if (ch == (unsigned char)'M') { uartNowEvents |= _M_; goto TELL_WAIT; }
    if (ch == (unsigned char)'W') { uartNowEvents |= _W_; goto TELL_WAIT; }
TELL_WAIT:
    e = (struct pool_t *)malloc(sizeof(struct pool_t));
    memcpy(&e->c, &ch, sizeof(ch));
    pthread_mutex_lock(&mPoolEmpty); pthread_cleanup_push(CleanupLock, (void *)&mPoolEmpty);
    list_add_tail(&e->list, &pl.list);
    poolEmpty = 0; pthread_cond_signal(&cPoolEmpty);
    pthread_cleanup_pop(1);
    return;
}
int main (int argc, char **argv) {
    struct termios term, save;
    int res, v, maxfd = 0;
    fd_set rset;
    struct timeval tv;
    struct timespec start_time, now_time;
    unsigned long long int start, now;
    struct uinput_user_dev uidev = {0};
    int i = 0;
    char line[512];

    uinputfd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (0 > uinputfd)                                    exit(1);
    if (ioctl(uinputfd, UI_SET_EVBIT, EV_KEY) < 0)        exit(2);
    for (i = 0; i < KEYPAD_KEYNUMBER; i++) {
        if (ioctl(uinputfd, UI_SET_KEYBIT, keypad_codes[i].code) < 0)
            exit(3);
    }
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "uinput-serial");
    uidev.id.bustype = BUS_VIRTUAL, uidev.id.vendor  = 0x1, uidev.id.product = 0x1, uidev.id.version = 1;
    if (write(uinputfd, &uidev, sizeof(uidev)) < 0)        exit(4);
    if(ioctl(uinputfd, UI_DEV_CREATE) < 0)                exit(5);
    sleep(1);

    ttyfd = open(TTY_SERIAL, O_RDWR | O_NOCTTY | O_NDELAY);
    if (0 > ttyfd)                                        exit(6);
    memset(&term, 0, sizeof(struct termios));
    if (0 > tcgetattr(ttyfd, &term))                    close(ttyfd), ttyfd = -1, exit(7);
    memcpy((void *)&save, (void *)&term, sizeof(struct termios));
    term.c_cflag |= B115200, term.c_cflag |= CLOCAL, term.c_cflag |= CREAD, term.c_cflag &= ~PARENB, term.c_cflag &= ~CSTOPB;
    term.c_cflag &= ~CSIZE, term.c_cflag |= CS8, term.c_iflag = IGNPAR, term.c_cc[VMIN] = 1, term.c_cc[VTIME] = 0;
    term.c_iflag = 0, term.c_oflag = 0, term.c_lflag = 0;
    cfsetispeed(&term, B115200), cfsetospeed(&term, B115200);
    if (0 > tcsetattr(ttyfd, TCSANOW, &term))        close(ttyfd), ttyfd = -1, exit(8);
    if (0 > (v = fcntl(ttyfd, F_GETFL, 0)))            close(ttyfd), ttyfd = -1, exit(9);
    if (0 > fcntl(ttyfd, F_SETFL, v | O_NONBLOCK))    close(ttyfd), ttyfd = -1, exit(10);
    if (0 > tcflush(ttyfd, TCIFLUSH))                close(ttyfd), ttyfd = -1, exit(11);
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    start = start_time.tv_sec * 1000 + start_time.tv_nsec / 1000000;

    INIT_LIST_HEAD(&pl.list);
    pthread_cond_init(&cPoolEmpty, NULL);
    pthread_create(&tEmit, NULL, EmitThread, 0);
    for (;;) {
        FD_ZERO(&rset);
        FD_SET(ttyfd, &rset);
        maxfd = 0;
        if (maxfd < ttyfd)            maxfd = ttyfd;
        memset(&tv, 0, sizeof(tv));
        tv.tv_sec = SELECT_EXPIRY_NINISECONDS / 1000;
        tv.tv_usec = (SELECT_EXPIRY_NINISECONDS % 1000) * 1000;
        //res = select(maxfd + 1, &rset, 0, 0, &tv);
        res = select(maxfd + 1, &rset, 0, 0, 0);
        if (0 > res)                close(ttyfd), ttyfd = -1, exit(12);
        if (0 == res)                continue;
        if (FD_ISSET(ttyfd, &rset)) {
            clock_gettime(CLOCK_MONOTONIC, &now_time);
            now = now_time.tv_sec * 1000 + now_time.tv_nsec / 1000000;
            res = read(ttyfd, (void *)line, sizeof(line));
            if (0 >= res)            close(ttyfd), ttyfd = -1, exit(13);
            for (i = 0; i < res; i++) {
                onUartChar(line[i]);
            }
            if (now - start >= SELECT_EXPIRY_NINISECONDS) {
            }
        }
    }
    tcsetattr(ttyfd, TCSANOW, &save);
    close(ttyfd), ttyfd = -1;

    sleep(1);
    ioctl(uinputfd, UI_DEV_DESTROY);
    close(uinputfd), uinputfd = -1;
    return 0;
}
