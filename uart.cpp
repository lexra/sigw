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

static int ttyfd = -1;
static int thread_running = 0;

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
	*p = sum;
	p++;
	return (p - buffer);
}

int uartKidVerify(int shutdown, int timeout) {
    unsigned char buffer[64] = {0};
	unsigned char data[16] = {0};
	int len;

	if (-1 == ttyfd)
		return 0;

	data[0] = (shutdown & 0xff);
	data[1] = (timeout & 0xff);

	len = makePacket(PACKET_HEADER, KID_VERIFY, 2, data, buffer);
    return write(ttyfd, buffer, len);
}

static int processNote(int len, unsigned char *data) {
	unsigned char nid;
	unsigned short state;

	nid = data[0];
	switch (nid) {
	case NID_FACE_STATE:
		state = BUILD_UINT16(data[1], data[2]);
		printf("NID_FACE_STATE, nid=0x%02x, state=0x%04x\n", nid, state);
		if (0 == state) {
			break;
		}
		if (1 == state) {
			break;
		}
		if (2 == state) {
			break;
		}
		if (3 == state) {
			break;
		}
		if (4 == state) {
			break;
		}
		if (5 == state) {
			break;
		}
		if (6 == state) {
			break;
		}
		if (7 == state) {
			break;
		}
		if (8 == state) {
			break;
		}
		if (9 == state) {
			break;
		}
		if (10 == state) {
			break;
		}
		if (11 == state) {
			printf("Direcion error. \n");
			break;
		}
		break;

	case NID_READY:
		printf("NID_READY\n");
		break;

	case NID_UNKNOWN_ERROR:
		printf("NID_UNKNOWN_ERROR\n");
		break;

	case NID_OTA_DONE:
		printf("NID_OTA_DONE\n");
		break;

	default:
		printf("NID_DEFAULT(%02x)\n", nid);
                break;
	}
	return 0;
}

static void processReply(int len, unsigned char *data) {
	unsigned char kid;
	unsigned char result;
	unsigned short user_id;
	unsigned char face_direction;

	kid = data[0];
	result = data[1];
	switch (kid) {
	case KID_FACE_RESET:
		/*user_id = BUILD_UINT16(data[3], data[2]);
		face_direction = data[4];
		printf("KID_ENROLL, result=%02x, face_direction=%02x\n", result, face_direction);
		if (MR_SUCCESS == result) {
			break;
		}
		if (MR_REJECTED == result) {
			break;
		}
		if (MR_ABORTED == result) {
			break;
		}
		if (MR_FAILED4_CAMERA == result) {
			break;
		}
		if (MR_FAILED4_UNKNOWNREASON == result) {
			break;
		}
		if (MR_FAILED4_INVALIDPARAM == result) {
			break;
		}
		if (MR_FAILED4_TIMEOUT == result) {
			break;
		}*/
		break;

	case KID_GET_SAVED_IMAGE:
		break;

	case KID_VERIFY:
		printf("KID_RESET\n");
		break;

	case KID_RESET:
		printf("KID_RESET\n");
		break;

	default:
		printf("KID_DEFAULT, kid=%02x, result=%d\n", kid, result);
		break;
	}

	return;
}

static int processPacket(int len, unsigned char *message) {
	unsigned char mid;
	int size;

	mid = message[0];
	size = BUILD_UINT16(message[2], message[1]);
	switch (mid) {
	case 0x00:
		printf("RID_REPLY\n");
			processReply(size, &message[3]);
		break;
	case 0x01:
		printf("RID_NOTE\n");
		processNote(size, &message[3]);
		break;
	case 0x02:
		printf("RID_IMAGE\n");
		break;
	case 0x03:
		printf("RID_ERROR\n");
		break;
	default:
		printf("RID_DEFAULT\n");
		break;
	}
	return 0;
}

static void onUartChar(int fd, unsigned char ch) {
	static int size = 0;
	int offset = 0;
	unsigned char sum;
	static unsigned char recv_packet[1024 * 8 + 6] = {0};
	static unsigned char *p = recv_packet;

	offset = p - recv_packet;
	if (0 == offset && *p != 0xef) {
		p = recv_packet;
		return;
	}
	if (1 == offset && *p != 0xaa) {
		p = recv_packet;
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
		processPacket(3 + size, &recv_packet[2]);
	}
	p = recv_packet;
	return;
}

static int onUartMsg(int id, int len, char *msg) {
	int fd;
	int length;
	char buffer[1028] = {0};
	int i;

	memcpy((void *)&fd, (void *)msg, sizeof(int));
	length = len - sizeof(int);
	memcpy(buffer, (void *)(msg + sizeof(int)), length);
	for (i = 0; i < length; i++)
		onUartChar(fd, (unsigned char)buffer[i]);
	return i;
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
	char line[1028] = {0};
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
			memcpy((void *)line, (void *)&ttyfd, sizeof(int));
            res = read(ttyfd, (void *)(line + sizeof(int)), sizeof(line) - sizeof(int));
            if (0 >= res)
				break;
            for (i = 0; i < res; i++) {
				sendEvent(MSG_UART_RCV, res + sizeof(int), line, onUartMsg);
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
