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
#include "timer.h"
#include "tcpsvc.h"

static int ttyfd = -1;
static int thread_running = 0;
static unsigned short face_state;

///////////////////////////////////////////////////////////
//

unsigned char checkSum(int n, const unsigned char *s) {
    unsigned char answer = 0;
    int i;

    for (i = 0; i < n; ++i)
        answer ^= s[i];
    return answer;
}

int makePacket(unsigned short header, unsigned char id, unsigned short length, unsigned char data[], unsigned char *buffer) {
	unsigned char *p = buffer;
	int i = 0;
	unsigned char sum = 0;

	*p = HI_UINT16(header); p++;
	*p = LO_UINT16(header); p++;
	*p = id; p++;
	*p = HI_UINT16(length); p++;
	*p = LO_UINT16(length); p++;
	if (length > 0) {
		for (i = 0; i < (int)length; i++) {
			*p = data[i];
			p++;
		}
	}
	sum = checkSum(3 + (int)length, &buffer[2]);
//printf("(%s %d) sum=%02x\n", __FILE__, __LINE__, sum);

	*p = sum;
	p++;
	return (p - buffer);
}

int uartKidVerify(int shutdown, int timeout) {
    unsigned char buffer[64] = {0};
	unsigned char data[16] = {0};
	int len;
	int res;

	if (-1 == ttyfd)
		return 0;

	data[0] = (shutdown & 0xff);
	data[1] = (timeout & 0xff);

	len = makePacket(PACKET_HEADER, KID_VERIFY, 2, data, buffer);
//printf("(%s %d) uartKidVerify=%02x\n", __FILE__, __LINE__, len);

	res = write(ttyfd, buffer, len);
	assert(res == len);
	return 8;
}

int uartKidPowerOn(void) {
	int i;
	unsigned char zero = 0;
	int res;

	if (-1 == ttyfd)
		return 0;
	for (i = 0; i < 1500; i++) {
		res = write(ttyfd, &zero, 1);
		assert(1 == res);
	}

	return 1500;
}

static void processKidNote(int len, unsigned char *data) {
	unsigned char rid = data[0];
	unsigned char nid = data[3];
	int length = len - 3;
	int i = 0;
	unsigned short state = 0;

	switch (nid) {
	case NID_READY:
		printf("(%s %d) NID_READY(%02x): ", __FILE__, __LINE__, length);

		for (i = 0; i < length; i++) {
			printf("%02x ", data[4 + i]);
		}
		printf("\n");

///////////////////////////////////////////////////////////
//
		//setTimer(TIMER_KID_VERIFY, 1000, sendKidTimer);
		uartKidVerify(0, 0x02);
		break;

	case NID_UNKNOWN_ERROR:
		printf("NID_UNKNOWN_ERROR(%02x): ", length);
		for (i = 0; i < length; i++) {
			printf("%02x ", data[4 + i]);
		}
		printf("\n");
		break;

	case NID_OTA_DONE:
		printf("NID_OTA_DONE\n");
		break;

	case NID_MASS_DATA_DONE:
		printf("NID_MASS_DATA_DONE(%02x): ", length);
		for (i = 0; i < length; i++) {
			printf("%02x ", data[4 + i]);
		}
		printf("\n");
		break;

	case NID_FACE_STATE: {
		face_state = BUILD_UINT16(data[4], data[5]);

		if (FACE_STATE_NOFACE == face_state) {
			printf("FACE_STATE_NOFACE\n");
			break;
		}
		if (FACE_STATE_TOOUP == face_state) {
			printf("FACE_STATE_TOOUP\n");
			break;
		}
		if (FACE_STATE_TOODOWN == face_state) {
			printf("FACE_STATE_TOODOWN\n");
			break;
		}
		if (FACE_STATE_TOOLEFT == face_state) {
			printf("FACE_STATE_TOOLEFT\n");
			break;
		}
		if (FACE_STATE_TOORIGHT == face_state) {
			printf("FACE_STATE_TOORIGHT\n");
			break;
		}
		if (FACE_STATE_TOOFAR == face_state) {
			printf("FACE_STATE_TOOFAR\n");
			break;
		}
		if (FACE_STATE_TOOCLOSE == face_state) {
			printf("FACE_STATE_TOOCLOSE\n");
			break;
		}
		if (FACE_STATE_FACE_OCCLUSION == face_state) {
			printf("FACE_STATE_FACE_OCCLUSION\n");
			break;
		}
		if (FACE_STATE_EYE_CLOSE_STATUS_OPEN_EYE == face_state) {
			printf("FACE_STATE_EYE_CLOSE_STATUS_OPEN_EYE\n");
			break;
		}
		if (FACE_STATE_EYE_CLOSE_STATUS == face_state) {
			printf("FACE_STATE_EYE_CLOSE_STATUS\n");
			break;
		}
		if (FACE_STATE_EYE_CLOSE_UNKNOW_STATUS == face_state) {
			printf("FACE_STATE_EYE_CLOSE_UNKNOW_STATUS\n");
			break;
		}
		printf("FACE_STATE UNTRAP\n");
		assert(0);
		break;
	}

	default:
		printf("(%s %d) UNTRAP NID=%02x\n", __FILE__, __LINE__, nid);
		break;
	}

	return;
}

void sendKidTimer(UINT nId) {
	if (TIMER_KID_POWER_ON == nId) {
		uartKidPowerOn();
		return;
	}
	if (TIMER_KID_VERIFY == nId) {
		uartKidVerify(0x00, 0x02);
		//uartKidVerify(0x01, 0x02);
		return;
	}
	return;
}

static void processKidReply(int len, unsigned char *data) {
	int n = 0;
	int list[CONCURRENT_CLIENT_NUMBER] = {0};

	unsigned char rid = data[0];
	unsigned char kid = data[3];
	unsigned char result = data[4];
	int length = len - 3;
	int i = 0;
	unsigned short user_id = 0;
	char user_name[64] = {0};
	unsigned char admin = 0, unlockStatus = 0;
	unsigned long long int lv_vals = 0;
	char yaml[4096] = {0};
	char tmp[512] = {0};

	switch (kid) {
	case KID_DEVICE_INFO:
		if (MR_REJECTED == result) {
			assert(2 == length);
			printf("KID_DEVICE_INFO: MR_REJECTED=%02x\n", data[5]);

///////////////////////////////////////////////////////////
//
			//setTimer(TIMER_KID_VERIFY, 1000, sendKidTimer);
			uartKidVerify(0, 0x02);
			break;
		}
		printf("KID_DEVICE_INFO(%02x): ", length);
		for (i = 0; i < length; i++) {
			printf("%02x ", data[4 + i]);
		}
		printf("\n");
		break;

	case KID_POWERDOWN:
		printf("KID_POWERDOWN(%02x): ", length);
		for (i = 0; i < length; i++) {
			printf("%02x ", data[4 + i]);
		}
		printf("\n");
		break;

	case KID_VERIFY:
		if (MR_ABORTED == result) {
			memset(yaml, 0, sizeof(yaml));
			strcat(yaml, "---\n");
			strcat(yaml, "verify\n");
			sprintf(tmp, "  id: -1\n"), strcat(yaml, tmp);
			sprintf(tmp, "  name: undef\n"), strcat(yaml, tmp);
			sprintf(tmp, "  result: MR_ABORTED\n"), strcat(yaml, tmp);
			sprintf(tmp, "\n\n"), strcat(yaml, tmp);
			n = tcpsGetConnectionList(list);
			if (n > 0)
				ipcSendto(n, list, yaml, strlen(yaml));
			setTimer(TIMER_KID_POWER_ON, 2000, sendKidTimer);
			break;
		}
		if (MR_FAILED_INVALID_PARAM == result) {
			memset(yaml, 0, sizeof(yaml));
			strcat(yaml, "---\n");
			strcat(yaml, "verify\n");
			sprintf(tmp, "  id: -1\n"), strcat(yaml, tmp);
			sprintf(tmp, "  name: undef\n"), strcat(yaml, tmp);
			sprintf(tmp, "  result: MR_FAILED_INVALID_PARAM\n"), strcat(yaml, tmp);
			sprintf(tmp, "\n\n"), strcat(yaml, tmp);
			n = tcpsGetConnectionList(list);
			if (n > 0)
				ipcSendto(n, list, yaml, strlen(yaml));
			setTimer(TIMER_KID_POWER_ON, 2000, sendKidTimer);
			break;
		}
		if (MR_FAILED_UNKNOWN_REASON == result) {
			printf("(%s %d) MR_FAILED_UNKNOWN_REASON\n", __FILE__, __LINE__);
			memset(yaml, 0, sizeof(yaml));
			strcat(yaml, "---\n");
			strcat(yaml, "verify\n");
			sprintf(tmp, "  id: -1\n"), strcat(yaml, tmp);
			sprintf(tmp, "  name: undef\n"), strcat(yaml, tmp);
			sprintf(tmp, "  result: MR_FAILED_UNKNOWN_REASON\n"), strcat(yaml, tmp);
			sprintf(tmp, "\n\n"), strcat(yaml, tmp);
			n = tcpsGetConnectionList(list);
			if (n > 0)
				ipcSendto(n, list, yaml, strlen(yaml));
			setTimer(TIMER_KID_POWER_ON, 2000, sendKidTimer);
			break;
		}
		if (MR_FAILED_UNKNOWN_USER == result) {
			printf("(%s %d) MR_FAILED_UNKNOWN_USER\n", __FILE__, __LINE__);
			memset(yaml, 0, sizeof(yaml));
			strcat(yaml, "---\n");
			strcat(yaml, "verify\n");
			sprintf(tmp, "  id: -1\n"), strcat(yaml, tmp);
			sprintf(tmp, "  name: undef\n"), strcat(yaml, tmp);
			sprintf(tmp, "  result: MR_FAILED_UNKNOWN_USER\n"), strcat(yaml, tmp);
			sprintf(tmp, "\n\n"), strcat(yaml, tmp);
			n = tcpsGetConnectionList(list);
			if (n > 0)
				ipcSendto(n, list, yaml, strlen(yaml));
			setTimer(TIMER_KID_POWER_ON, 2000, sendKidTimer);
			break;
		}
		if (MR_FAILED_LIVENESS_CHECK == result) {
			printf("(%s %d) MR_FAILED_LIVENESS_CHECK\n", __FILE__, __LINE__);
			memset(yaml, 0, sizeof(yaml));
			strcat(yaml, "---\n");
			strcat(yaml, "verify\n");
			sprintf(tmp, "  id: -1\n"), strcat(yaml, tmp);
			sprintf(tmp, "  name: undef\n"), strcat(yaml, tmp);
			sprintf(tmp, "  result: MR_FAILED_LIVENESS_CHECK\n"), strcat(yaml, tmp);
			sprintf(tmp, "\n\n"), strcat(yaml, tmp);
			n = tcpsGetConnectionList(list);
			if (n > 0)
				ipcSendto(n, list, yaml, strlen(yaml));
			setTimer(TIMER_KID_POWER_ON, 2000, sendKidTimer);
			break;
		}
		if (MR_FAILED_DEV_OPEN_FAIL == result) {
			printf("(%s %d) MR_FAILED_DEV_OPEN_FAIL\n", __FILE__, __LINE__);
			memset(yaml, 0, sizeof(yaml));
			strcat(yaml, "---\n");
			strcat(yaml, "verify\n");
			sprintf(tmp, "  id: -1\n"), strcat(yaml, tmp);
			sprintf(tmp, "  name: undef\n"), strcat(yaml, tmp);
			sprintf(tmp, "  result: MR_FAILED_DEV_OPEN_FAIL\n"), strcat(yaml, tmp);
			sprintf(tmp, "\n\n"), strcat(yaml, tmp);
			n = tcpsGetConnectionList(list);
			if (n > 0)
				ipcSendto(n, list, yaml, strlen(yaml));
			setTimer(TIMER_KID_POWER_ON, 2000, sendKidTimer);
			break;
		}
		if (MR_FAILED_TIME_OUT == result) {
			printf("(%s %d) MR_FAILED_TIME_OUT\n", __FILE__, __LINE__);
			memset(yaml, 0, sizeof(yaml));
			strcat(yaml, "---\n");
			strcat(yaml, "verify\n");
			sprintf(tmp, "  id: -1\n"), strcat(yaml, tmp);
			sprintf(tmp, "  name: undef\n"), strcat(yaml, tmp);
			sprintf(tmp, "  result: MR_FAILED_TIME_OUT\n"), strcat(yaml, tmp);
			sprintf(tmp, "\n\n"), strcat(yaml, tmp);
			n = tcpsGetConnectionList(list);
			if (n > 0)
				ipcSendto(n, list, yaml, strlen(yaml));
			setTimer(TIMER_KID_POWER_ON, 2000, sendKidTimer);
			break;
		}
		user_id = BUILD_UINT16(data[6], data[5]);
		for (i = 0; i < 32; i++)
			user_name[i] = (char)data[7 + i];
		admin = data[39], unlockStatus = data[40];
		if (ST_FACE_MODULE_STATUS_UNLOCK_OK == unlockStatus)
			printf("ST_FACE_MODULE_STATUS_UNLOCK_OK, user_id=%04x, user_name=%s, admin=%02x\n", user_id, user_name, admin);
		else //if (ST_FACE_MODULE_STATUS_UNLOCK_WITH_EYES_CLOSE == unlockStatus)
			printf("ST_FACE_MODULE_STATUS_UNLOCK_WITH_EYES_CLOSE, user_id=%04x, user_name=%s, admin=%02x\n", user_id, user_name, admin);
		if (len > 41) {
			memcpy((void *)&lv_vals, (void *)&data[41], sizeof(lv_vals));
			printf("lv_vals=%llu\n", lv_vals);
		}
		memset(yaml, 0, sizeof(yaml));
		strcat(yaml, "---\n");
		strcat(yaml, "verify\n");
		sprintf(tmp, "  id: %02x\n", user_id), strcat(yaml, tmp);
		sprintf(tmp, "  name: %s\n", user_name), strcat(yaml, tmp);
		sprintf(tmp, "  result: MR_SUCCESS\n"), strcat(yaml, tmp);
		if (ST_FACE_MODULE_STATUS_UNLOCK_OK == unlockStatus)
			sprintf(tmp, "  status: ST_FACE_MODULE_STATUS_UNLOCK_OK\n"), strcat(yaml, tmp);
		else
			sprintf(tmp, "  status: ST_FACE_MODULE_STATUS_UNLOCK_WITH_EYES_CLOSE\n"), strcat(yaml, tmp);
		sprintf(tmp, "\n\n"), strcat(yaml, tmp);
		n = tcpsGetConnectionList(list);
		if (n > 0)
			ipcSendto(n, list, yaml, strlen(yaml));
		setTimer(TIMER_KID_POWER_ON, 2000, sendKidTimer);
		//setTimer(TIMER_KID_VERIFY, 1000, sendKidTimer);
		break;

	default:
		printf("(%s %d) UNTRAP KID=%02x\n", __FILE__, __LINE__, kid);
		break;
	}
	return;
}

static int processKidPacket(int len, unsigned char *message) {
	unsigned char rid;
	int size;

	rid = message[0];
	size = BUILD_UINT16(message[2], message[1]);

	switch (rid) {
	case RID_REPLY:
		processKidReply(size + 3, &message[0]);
		break;

	case RID_NOTE:
		processKidNote(size + 3, &message[0]);
		break;

	case RID_IMAGE:
		printf("RID_IMAGE\n");
		break;

	case RID_ERROR:
		printf("RID_ERROR\n");
		break;

	default:
		printf("UNTRAPED RID\n");
		break;
	}
	return 0;
}

// 9f dc 00 00 02 00 01 03
static void onUartChar(int fd, unsigned char ch) {
	static unsigned char recv_packet[1024 * 8 + 6] = {0};
	static unsigned char *p = recv_packet;
	static int size = 0;

	int offset = 0;
	unsigned char sum;

	*p = ch;
	offset = p - recv_packet;

	if (0 == offset && *p != 0x9f) {
		p = recv_packet;
		size = 0;
		return;
	}
	if (1 == offset && *p != 0xdc) {
		p = recv_packet;
		size = 0;
		return;
	}
	if (4 == offset) {
		size = BUILD_UINT16(*p, *(p - 1));
	}
	if (0 == size) {
		p++;
		return;
	}
	if (offset < size + 5) {
		p++;
		return;
	}

	sum = checkSum(3 + size, &recv_packet[2]);
	if (*p == sum) {
		processKidPacket(3 + size, &recv_packet[2]);
	}
	p = recv_packet;
	size = 0;
	return;
}

void tellUartThreadExit(void) {
	thread_running = 0;
	return;
}

int getUartDescriptor(void) {
	return ttyfd;
}

void *uartThread(void *param) {
	struct termios term = {0}, save = {0};
    int i, res, v, maxfd = 0;
    fd_set rset;
	unsigned char line[1024 + 4] = {0};
	struct timeval tv = {0};

	assert(0 != param);
	memcpy(&ttyfd, param, sizeof(int));

	assert(-1 != ttyfd);
	assert(tcgetattr(ttyfd, &term) >= 0);

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
            res = read(ttyfd, (void *)line, sizeof(line));
            for (i = 0; i < res; i++) {
				onUartChar(ttyfd, line[i]);
            }
        }
	}

	printf("(%s %d) uartThread() return\n", __FILE__, __LINE__);
	if (-1 == ttyfd)
		return 0;
    tcsetattr(ttyfd, TCSANOW, &save);
    close(ttyfd), ttyfd = -1;
	return 0;
}

static void onIpcYaml(int fd, int len, char *buffer) {
	char reqYaml[1024 * 4] = {0};
	char yaml[1024 * 4] = {0};
    int r = 0;
    char path[256] = {0};
	FILE *f = 0;
    char line[512] = {0};
    char request[512] = {0};
    char tmp[512] = {0};

	int gpio_num = 57;
	int gpio_value = 1;
	struct stat lbuf = {0};

	int n = 0;
	int list[CONCURRENT_CLIENT_NUMBER] = {0};

	memcpy(reqYaml, (void *)buffer, len);
	printf("%s", reqYaml);

    while (0 == r)
        srand(time(NULL)), r = rand();
    sprintf(path, "/tmp/ipc-%08X.yaml", r);
    f = fopen(path, "wb"), assert(0 != f);
    fwrite(reqYaml, 1, len, f);
    fclose(f), f = 0;

	f = fopen(path, "r"), assert(0 != f);
	fseek(f, 0, SEEK_SET);
	fgets(line, sizeof(line), f);
	fgets(request, sizeof(line), f);
	if (0 == strncmp("gpio", request, 4)) {
		fgets(line, sizeof(line), f);
		sscanf(line, "  %d: %d", &gpio_num, &gpio_value);
	}
    fclose(f), f = 0;
	unlink(path);

	if (0 == strncmp("gpio", request, 4)) {
		sprintf(path, "/sys/class/gpio/gpio%d/value", gpio_num);
		if (0 == lstat(path, &lbuf)) {
			f = fopen(path, "w"), assert(0 != f);
			sprintf(line, "%d", gpio_value);
			fclose(f), f = 0;

			memset(yaml, 0, sizeof(yaml));
			strcat(yaml, "---\n");
			strcat(yaml, "gpio:\n");
			sprintf(tmp, "  %d: %d\n", gpio_num, gpio_value), strcat(yaml, tmp);
			sprintf(tmp, "\n\n"), strcat(yaml, tmp);
			n = tcpsGetConnectionList(list);
			if (n > 0)
				ipcSendto(n, list, yaml, strlen(yaml));
		}
		return;
	}

	return;
}

static void onIpcChar(int fd, char ch) {
	static char recv_packet[1024 * 4] = {0};
	static char *p = recv_packet;

	int offset = 0;
	unsigned char sum;

	*p = ch;
	offset = p - recv_packet;

	if (offset > 0 && 0x0a == *p && 0x0a == *(p - 1)) {
		onIpcYaml(fd, offset + 1, recv_packet);
		p = recv_packet;
		memset(recv_packet, 0, sizeof(recv_packet));
		return;
	}
	if (offset > 2 && 
		0x0d == *(p - 1) && 0x0a == *(p - 0) 
		&& 0x0d == *(p - 3) && 0x0a == *(p - 2)
	) {
		onIpcYaml(fd, offset + 1, recv_packet);
		p = recv_packet;
		memset(recv_packet, 0, sizeof(recv_packet));
		return;
	}
	if (0 == offset && '-' != recv_packet[0]) {
		p = recv_packet;
		memset(recv_packet, 0, sizeof(recv_packet));
		return;
	}
	if (1 == offset && '-' != recv_packet[0] && '-' != recv_packet[1]) {
		p = recv_packet;
		memset(recv_packet, 0, sizeof(recv_packet));
		return;
	}
	if (2 == offset && '-' != recv_packet[0] && '-' != recv_packet[1] && '-' != recv_packet[2]) {
		p = recv_packet;
		memset(recv_packet, 0, sizeof(recv_packet));
		return;
	}
	if (3 == offset && '-' != recv_packet[0] && '-' != recv_packet[1] && '-' != recv_packet[2] && 0x0a != recv_packet[3] && 0x0d != recv_packet[3]) {
		p = recv_packet;
		memset(recv_packet, 0, sizeof(recv_packet));
		return;
	}
	p++;
	return;
}

int onIpcBuffer(int fd, int length, char *buffer) {
	int i = 0;

//printf("(%s %d) onIpcBuffer(%d)=%d\n", __FILE__, __LINE__, fd, length);
//printf("(%s %d) %s\n", __FILE__, __LINE__, buffer);
//	for (i = 0; i < length; i++)
//		printf("%02x ", buffer[i]);
//	printf("\n");

	for (i = 0; i < length; i++)
		onIpcChar(fd, buffer[i]);
	return length;
}

