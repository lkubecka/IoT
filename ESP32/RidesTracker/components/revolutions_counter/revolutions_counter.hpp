#ifndef REVOLUTIONS_COUNTER_H
#define REVOLUTIONS_COUNTER_H

#include <sys/types.h>
#include <sys/time.h>
#include "esp_err.h"
#include "driver/pcnt.h"

extern xQueueHandle counterEventQueue;

class RevolutionsCounter {
    private:
		const int  RECORD_NTH_ROTATION = 10;
		const int  _msgQueueSize = 10;

		pcnt_unit_t _pcntUnit = PCNT_UNIT_0;
		int _upperLimit = 32767;
		int _lowerLimit = -1;
		int _firstTresholdValue = 100;
		int _secondTresholdValue = RECORD_NTH_ROTATION;
		gpio_num_t _reedPin = GPIO_NUM_13;
		int _samplesToIgnore = 1000;	
		void _initPcnt(void);	
	public:
	    /* A structure to pass events from the PCNT interrupt handler to the main program.*/
		typedef struct {
			struct  timeval time;
			uint32_t status; // information on the event type that caused the interrupt
		} pcnt_evt_t;

		RevolutionsCounter(gpio_num_t reedPin);
		~RevolutionsCounter();
		int16_t getNumberOfRevolutions(void) const;
		void disable(void);
		void enable(void);
};

#endif  // REVOLUTIONS_COUNTER_H
