
#ifndef __TCPSVC__
#define __TCPSVC__


#define LISTEN_PORT							34000
#define SELECT_EXPIRY_SECONDS				1
#define CONCURRENT_CLIENT_NUMBER			12

int get_connection_list(int list[]);
void tell_tcpsvc_quit(void);

void *tcpsvc_thread(void *param);


#endif // 