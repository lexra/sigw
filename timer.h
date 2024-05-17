
#ifndef __TIMER__
#define __TIMER__

#include <sys/timeb.h>


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


#define MAX_TIMERS			64
#define TIMER_ID_PERIOD		0

typedef void (*TIMER_FUNC)(UINT nId);

typedef struct Timer_t
{
	UINT id;
	unsigned int timeout;
	TIMER_FUNC cb;
	unsigned int enable;
} Timer_t;


UINT setTimer(UINT nId, UINT nElapse, TIMER_FUNC cb);
BOOL killTimer(UINT nId);

void PollTimer(void);

#endif //__TIMER__