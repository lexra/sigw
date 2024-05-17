
#ifndef __TCPSVC__
#define __TCPSVC__


#define LISTEN_PORT							34000
#define SELECT_EXPIRY_SECONDS				1
#define CONCURRENT_CLIENT_NUMBER			12

int tcpsGetConnectionList(int list[]);
void tellTcpsExit(void);
void *tcpsThread(void *param);

#endif // 
