#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "mp4event.h"

static xSemaphoreHandle HTTPFLAGSMUX;


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


int mp4Event_Init()
{
	HTTPFLAGSMUX = xSemaphoreCreateMutex();
	if (HTTPFLAGSMUX == NULL) {
		return -1;
	}
	return 0;
}
