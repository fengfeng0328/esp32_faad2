#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "mp4event.h"

int pHeadCnt = 0;
int pMdatCnt = 0;

static xSemaphoreHandle HTTPFLAGSMUX;
static xSemaphoreHandle MP4REQUEST_TIMERMUX;


/* HTTP REQUEST STATE */
static HTTPFLAGS HFLAG = HTTPEXIT;


HTTPFLAGS GET_HTTPFLAGS()
{
	xSemaphoreTake(HTTPFLAGSMUX, portMAX_DELAY);
	HTTPFLAGS HFLAG_T = HFLAG;
	xSemaphoreGive(HTTPFLAGSMUX);
	return HFLAG_T;
}
HTTPFLAGS SET_HTTPFLAGS(HTTPFLAGS HFLAG_T)
{
	xSemaphoreTake(HTTPFLAGSMUX, portMAX_DELAY);
	HFLAG = HFLAG_T;
	xSemaphoreGive(HTTPFLAGSMUX);
	return HFLAG;
}

static MP4REQUEST_TIMER MREQUEST_TIMER = MP4REQUEST_DEFAULT;

MP4REQUEST_TIMER GET_MP4REQUEST()
{
	xSemaphoreTake(MP4REQUEST_TIMERMUX, portMAX_DELAY);
	MP4REQUEST_TIMER MP4REQUEST_T = MREQUEST_TIMER;
	xSemaphoreGive(MP4REQUEST_TIMERMUX);
	return MP4REQUEST_T;
}
MP4REQUEST_TIMER SET_MP4REQUEST(MP4REQUEST_TIMER MREQUEST_TIMER_T)
{
	xSemaphoreTake(MP4REQUEST_TIMERMUX, portMAX_DELAY);
	MREQUEST_TIMER = MREQUEST_TIMER_T;
	xSemaphoreGive(MP4REQUEST_TIMERMUX);
	return MREQUEST_TIMER;
}


int mp4Event_Init()
{
	HTTPFLAGSMUX = xSemaphoreCreateMutex();
	if (HTTPFLAGSMUX == NULL) {
		return -1;
	}

	MP4REQUEST_TIMERMUX = xSemaphoreCreateMutex();
	if (MP4REQUEST_TIMERMUX == NULL) {
		return -1;
	}
	return 0;
}
