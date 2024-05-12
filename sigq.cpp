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

////////////////////////////////////////////////////////////
//
static char *stripCrLf(char *line) {
    int len = 0;

    if(line == 0)		return 0;
    len = strlen(line);
    if(len > 1000)		return line;
    if(len <= 0)		return line;
    if (line[len - 1] == '\n' || line[len - 1] == '\r')
        line[len - 1] = 0;
    len = strlen(line);
    if (len > 0)
        if (line[len - 1] == '\n' || line[len - 1] == '\r')
            line[len - 1] = 0;
    return line;
}
static int pidof(const char *name, pid_t *process) {
    regex_t Regx;
    char Pattern[128] = {0}, cmd[128] = {0}, line[128] = {0}, num[64] = {0};
    FILE *f = 0;
    regmatch_t Match[64];
    int len = 0, id = -1, I = 0;

    strcpy(Pattern, "^([0-9]{1,})$"), regcomp(&Regx, Pattern, REG_EXTENDED);
    sprintf(cmd, "pidof %s", name);
    f = popen(cmd, "r");
    if (0 == f)        return -1;
    fgets(line, sizeof(line), f);
    stripCrLf(line);
    if (0 != regexec(&Regx, line, sizeof(Match) / sizeof(regmatch_t), Match, 0)) {
        regfree(&Regx), pclose(f), f = 0;
        return -1;
    }
    I = 1, len = Match[I].rm_eo - Match[I].rm_so, memcpy(num, line + Match[I].rm_so, len), num[len] = 0;
    sscanf(num, "%d", &id);
    if (process)    *process = id;
    regfree(&Regx), pclose(f), f = 0;
    return 0;
}

int main(int argc, char *argv[]) {
    union sigval sigVal = {0};
    pid_t pid = -1;
    char msg[1024] = {0};
    int r = 0;
    char path[1024] = {0};
    FILE *f, *stream = stdin;
    char *p;
    int l;
	int i, res;

    if (3 != argc && 2 != argc)
        printf("Usage: echo HELLO | ./sigq sigw\n"), exit(1);
    if (0 != pidof(argv[1], &pid)) {
        exit(2);
        //printf("(%s %d) sigw not running\n", __FILE__, __LINE__), exit(2);
    }

    while (0 == r)
        srand(time(NULL)), r = rand();
    sprintf(path, "/tmp/msg-%08X.txt", r);
    f = fopen(path, "wb"), assert(0 != f);

    if (3 == argc)
		stream = fopen(argv[2], "rb"), assert(0 != stream);
	memset((p = msg), 0, sizeof(msg));
    while (1) {
        l = fread(p, sizeof(char), 128, stream);
        if (0 == l) break;
        p += l;
    }
    if (3 == argc)
    	fclose (stream), stream = 0;

    fwrite(msg, sizeof(char), (int)(p - msg), f);
    fclose(f), f = 0;

    memset(&sigVal, 0, sizeof(union sigval));
    sigVal.sival_int = r;

#if 1
	for(i = 0; i < 2; i++) {
		res = sigqueue(pid, SIGUSR1, sigVal);
		if (-1 != res)
			break;
	}
	if (-1 == res) {
		printf("(%s %d) sigqueue() error\n", __FILE__, __LINE__);
		unlink(path);
		exit(2);
	}
#else
	if(sigqueue(pid, SIGUSR1, sigVal) == -1) {
		printf("(%s %d) sigqueue() error\n", __FILE__, __LINE__);
		unlink(path);
		exit(2);
    }
#endif

    //printf("(%s %d) sigqueue(). \n", __FILE__, __LINE__);
    return 0;
}
