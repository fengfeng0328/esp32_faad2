#ifndef MP4EVENT_H
#define MP4EVENT_H

extern int pHeadCnt;
extern int pMdatCnt;

typedef enum MP4REQUEST_TIMER {
	MP4REQUEST_DEFAULT,	// 非MP4请求使用DEFAULT
	MP4REQUEST_FIRST, 	// 第一次请求mdat头部前数据
	MP4REQUEST_SECOND, 	// 第二次请求mdat头部尾数据
	MP4REQUEST_THIRD	// 第三次请求mdat音频帧数据
}MP4REQUEST_TIMER;

typedef enum HTTPFALGS {
	HTTPOPEN, HTTPCLOSE, HTTPEXIT
} HTTPFLAGS;

HTTPFLAGS GET_HTTPFLAGS();
HTTPFLAGS SET_HTTPFLAGS(HTTPFLAGS HFLAG_T);

int mp4Event_Init();

#endif
