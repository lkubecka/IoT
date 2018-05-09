
#ifndef REVOLUTION_COUNTER_H
#define REVOLUTION_COUNTER_H

#include <sys/types.h>
#include <sys/time.h>
#include "esp_err.h"
#include "driver/pcnt.h"

extern xQueueHandle pcnt_evt_queue;

/* A structure to pass events from the PCNT interrupt handler to the main program.*/
typedef struct {
	struct  timeval time;
	uint32_t status; // information on the event type that caused the interrupt
} pcnt_evt_t;

#ifdef __cplusplus
extern "C" {
#endif

void initPcnt(void);
int16_t getNumberOfRevolutions(void);

#ifdef __cplusplus
}
#endif

#endif  // REVOLUTION_COUNTER_H
