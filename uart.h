#ifndef __UART__
#define __UART__


#ifndef BOOL
 #define BOOL int
#endif

#ifndef INT
 #define INT int
#endif

#ifndef UINT
 #define UINT unsigned int
#endif

#ifndef DWORD
 #define DWORD unsigned long
#endif

#ifndef WPARAM
 #define WPARAM unsigned int
#endif

#ifndef LPARAM
 #define LPARAM unsigned int
#endif

#ifndef LRESULT
 #define LRESULT unsigned int
#endif


#ifndef BUILD_UINT16
 #define BUILD_UINT16(loByte, hiByte) \
          ((unsigned short)(((loByte) & 0x00FF) + (((hiByte) & 0x00FF) << 8)))

 #define BUILD_UINT32(Byte0, Byte1, Byte2, Byte3) \
          ((unsigned int)((unsigned int)((Byte0) & 0x00FF) + ((unsigned int)((Byte1) & 0x00FF) << 8) \
                        + ((unsigned int)((Byte2) & 0x00FF) << 16) + ((unsigned int)((Byte3) & 0x00FF) << 24)))

 #define HI_UINT16(a) (((a) >> 8) & 0xFF)
 #define LO_UINT16(a) ((a) & 0xFF)
#endif // BUILD_UINT16


#define PACKET_HEADER				0x9FDC

#define KID_DEVICE_INFO				0x00
#define KID_GET_VERSION				0x01
#define KID_SYS_INIT_READY_TIME		0x70
#define KID_POWERDOWN				0x0f
#define KID_RESET					0x0e
#define KID_GET_STATUS				0x10
#define KID_VERIFY					0x11
#define KID_ENROLL					0x12
#define KID_FACE_RESET				0x13
#define KID_ENROLL_SINGLE			0x14
#define KID_DEL_USER				0x1a
#define KID_DEL_ALL					0x1b
#define KID_GET_USER_INFO			0x1c
#define KID_GET_ALL_USER_ID			0x1d
#define KID_SNAP_IMAGE				0x20
#define KID_GET_SAVED_IMAGE			0x21
#define KID_UPLOAD_IMAGE			0x22

#define KID_SW_EXP_FM_MODE			0x26
#define KID_SW_EXP_DB_MODE			0x27
#define KID_EXP_FM_DATA				0x28
#define KID_EXP_DB_DATA				0x29
#define KID_IMP_DB_DATA				0x2a
#define KID_IMP_FM_DATA				0x2b

#define KID_CATCH_IMG_MODE			0x2d
#define KID_EXP_IMG_DATA			0x2e

#define KID_CONFIG_BAUDRATE			0x50

#define MR_SUCCESS					0
#define MR_REJECTED					1
#define MR_ABORTED					2 // algo aborted 
#define MR_FAILED_INVALID_CMD		4
#define MR_FAILED4_CAMERA			4 // camera open failed 
#define MR_FAILED_INVALID_PARAM		5
#define MR_FAILED4_UNKNOWNREASON	5 // UNKNOWN_ERROR 
#define MR_FAILED_TIME_OUT			6
#define MR_FAILED4_INVALIDPARAM		6 // invalid param 
#define MR_FAILED4_NOMEMORY			7 // no enough memory 
#define MR_FAILED_NOT_READY			8
#define MR_FAILED4_UNKNOWNUSER		8 // exceed limitation 
#define MR_SUCCESS_BUT_DEL_USER		9
#define MR_FAILED4_MAXUSER			9 // exceed maximum user number 
#define MR_FAILED4_FACEENROLLED		0x0a // this face has been enrolled 

#define MR_FAILED_UNKNOWN_USER		0x10
#define MR_FAILED_EXISTED_USER		0x11
#define MR_FAILED_LIVENESS_CHECK	0x14

#define MR_FAILED4_LIVENESSCHECK	0x0c // liveness check failed 
#define MR_FAILED4_TIMEOUT			0x0d // exceed the time limit 
#define MR_FAILED4_AUTHORIZATION	0x0e // authorization failed 
#define MR_FAILED_UNKNOWN_REASON	0x0f
#define MR_FAILED4_READ_FILE		0x13 // read file failed 
#define MR_FAILED4_WRITE_FILE		0x14 // write file failed 
#define MR_FAILED4_NO_ENCRYPT		0x15

#define MR_FAILED_DEV_OPEN_FAIL		0x40

#define RID_REPLY					0x00
#define RID_NOTE					0x01
#define RID_IMAGE					0x02
#define RID_ERROR					0x03

#define NID_READY					0x00
#define NID_FACE_STATE				0x01
#define NID_UNKNOWN_ERROR			0x02
#define NID_OTA_DONE				0x03
#define NID_MASS_DATA_DONE			0x04

#define FACE_STATE_NOFACE								1
#define FACE_STATE_TOOUP								2
#define FACE_STATE_TOODOWN								3
#define FACE_STATE_TOOLEFT								4
#define FACE_STATE_TOORIGHT								5
#define FACE_STATE_TOOFAR								6
#define FACE_STATE_TOOCLOSE								7
#define FACE_STATE_FACE_OCCLUSION						10
#define FACE_STATE_EYE_CLOSE_STATUS_OPEN_EYE			12
#define FACE_STATE_EYE_CLOSE_STATUS						13
#define FACE_STATE_EYE_CLOSE_UNKNOW_STATUS				14

#define ST_FACE_MODULE_STATUS_UNLOCK_OK					0xc8
#define ST_FACE_MODULE_STATUS_UNLOCK_WITH_EYES_CLOSE	0xcc


#define TTY_SERIAL						"/dev/ttyS1"
#define TTY_SERIAL_0					"/dev/ttyUSB0"
#define TTY_SERIAL_1					"/dev/ttyS1"


void tellUartThreadExit(void);

int getUartDescriptor(void);
void *uartThread(void *param);

unsigned char checkSum(int n, const unsigned char *s);
int makePacket(unsigned short header, unsigned char id, unsigned short length, unsigned char data[], unsigned char *buffer);

void sendKidTimer(UINT nId);
int uartKidVerify(int shutdown, int timeout);
int uartKidPowerOn(void);
int onIpcBuffer(int fd, int length, char *buffer);

#endif // 

// p b g w
// pin
// 9F DC 11 00 02 09 14 0E