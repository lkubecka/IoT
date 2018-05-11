#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/periph_ctrl.h"
#include "driver/gpio.h"
#include "driver/pcnt.h"
#include "esp_attr.h"
#include "soc/gpio_sig_map.h"
#include "revolutions_counter.hpp"
#include "time.hpp"

static const char* TAG = "RevolutionsCounter";


xQueueHandle counterEventQueue = NULL;   // A queue to handle pulse counter events
#define PCNT_UNIT PCNT_UNIT_0


/* Pass evet type to the main program using a queue. */
static void IRAM_ATTR handleReedInterrupt(void *arg)
{
	uint32_t intr_status = PCNT.int_st.val;
	RevolutionsCounter::pcnt_evt_t evt;
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
    
	if (intr_status & (BIT(PCNT_UNIT))) {

        
		evt.time = kalfy::time::getCurrentTime();
		evt.status = PCNT.status_unit[PCNT_UNIT].val;

		PCNT.int_clr.val = BIT(PCNT_UNIT);
		xQueueSendFromISR(counterEventQueue, &evt, &xHigherPriorityTaskWoken);

		if (evt.status & PCNT_STATUS_THRES0_M) {
			pcnt_counter_clear(PCNT_UNIT);
		}

		if (xHigherPriorityTaskWoken == pdTRUE) {
			portYIELD_FROM_ISR();
		}
	}
}

RevolutionsCounter::RevolutionsCounter(gpio_num_t reedPin) {
	_reedPin = reedPin;

	RevolutionsCounter::_initPcnt();
}

RevolutionsCounter::~RevolutionsCounter()
{
}

int16_t RevolutionsCounter::getNumberOfRevolutions(void) {
	int16_t pcntCntr = 0;
	if (pcnt_get_counter_value(_pcntUnit, &pcntCntr) != ESP_OK) {
		esp_log_write(ESP_LOG_ERROR, TAG, "Cannot read PCNT");
	}
	return pcntCntr;
}	

void RevolutionsCounter::disable(void) {
	pcnt_counter_pause(_pcntUnit);
	pcnt_intr_disable(_pcntUnit);
	esp_log_write(ESP_LOG_INFO, TAG, "Revolution counter stopped.");
}

void RevolutionsCounter::enable(void) {
	pcnt_intr_enable(_pcntUnit);
	pcnt_counter_resume(_pcntUnit);
	esp_log_write(ESP_LOG_INFO, TAG, "Revolution counter started.");
}

void RevolutionsCounter::_initPcnt(void)
{
	pcnt_config_t pcnt_config;
	pcnt_config.pulse_gpio_num = _reedPin;
	pcnt_config.ctrl_gpio_num = PCNT_PIN_NOT_USED;
	pcnt_config.channel = PCNT_CHANNEL_0;
	pcnt_config.unit = _pcntUnit;
	// What to do on the positive / negative edge of pulse input?
	pcnt_config.pos_mode = PCNT_COUNT_INC;      // Count up on the positive edge
	pcnt_config.neg_mode = PCNT_COUNT_DIS;      // Keep the counter value on the negative edge
												// What to do when control input is low or high?
	pcnt_config.lctrl_mode = PCNT_MODE_KEEP;    // Keep the primary counter mode if high
	pcnt_config.hctrl_mode = PCNT_MODE_KEEP;    // Keep the primary counter mode if high
												
	pcnt_config.counter_h_lim = _upperLimit; // Set the maximum limit values to watch
	pcnt_config.counter_l_lim = _lowerLimit; // Set the minimum limit values to watch

	/* Initialize PCNT unit */
	pcnt_unit_config(&pcnt_config);
	

	/* Configure and enable the input filter */
	pcnt_set_filter_value(_pcntUnit, _samplesToIgnore);
	pcnt_filter_enable(_pcntUnit);

	/* Set threshold 0 and 1 values and enable events to watch */
	pcnt_set_event_value(_pcntUnit, PCNT_EVT_THRES_1, _firstTresholdValue);
	pcnt_event_enable(_pcntUnit, PCNT_EVT_THRES_1);
	pcnt_set_event_value(_pcntUnit, PCNT_EVT_THRES_0, _secondTresholdValue);
	pcnt_event_enable(_pcntUnit, PCNT_EVT_THRES_0);
	/* Enable events on zero, maximum and minimum limit values */
	pcnt_event_enable(_pcntUnit, PCNT_EVT_ZERO);
	pcnt_event_enable(_pcntUnit, PCNT_EVT_H_LIM);
	pcnt_event_enable(_pcntUnit, PCNT_EVT_L_LIM);

	/* Initialize PCNT's counter */
	pcnt_counter_pause(_pcntUnit);
	pcnt_counter_clear(_pcntUnit);

	/* Register ISR handler and enable interrupts for PCNT unit */
	pcnt_isr_register(handleReedInterrupt, NULL, 0, NULL);

	enable();

	gpio_pullup_dis(_reedPin);
	gpio_pulldown_en(_reedPin);

	counterEventQueue = xQueueCreate(_msgQueueSize, sizeof(pcnt_evt_t));
	if (counterEventQueue == NULL) {
		esp_log_write(ESP_LOG_ERROR, TAG, "Error creating revolution message queue");
	}
}