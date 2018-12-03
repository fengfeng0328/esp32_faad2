#ifndef MP4EVENT_H
#define MP4EVENT_H

extern int pHeadCnt;
extern int pMdatCnt;

typedef enum HTTPFALGS {
	HTTPOPEN, HTTPCLOSE, HTTPEXIT
} HTTPFLAGS;

HTTPFLAGS GET_HTTPFLAGS();
HTTPFLAGS SET_HTTPFLAGS(HTTPFLAGS HFLAG_T);

int mp4Event_Init();

#endif
