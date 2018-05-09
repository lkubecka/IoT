#include "esp_err.h"
#include "revolution_counter.h"
#include "esp_log.h"


static const char* TAG = "RevolutionCounter";

xQueueHandle pcnt_evt_queue = NULL;   // A queue to handle pulse counter events
volatile uint8_t state = 0x01;

#define RECORD_NTH_ROTATION 10
#define REED_PIN GPIO_NUM_14
#define LED_BUILTIN GPIO_NUM_2
#define REVOLUTION_MSG_QUEUE_SIZE 10

const pcnt_unit_t PCNT_UNIT = PCNT_UNIT_0;
const int PCNT_H_LIM_VAL = 32767;
const int PCNT_L_LIM_VAL = -1;
const int PCNT_THRESH1_VAL = 200;
const int PCNT_THRESH0_VAL = RECORD_NTH_ROTATION;
const int PCNT_INPUT_SIG_IO = REED_PIN;

/* Pass evet type to the main program using a queue. */
static void IRAM_ATTR handleReedInterrupt(void *arg)
{
	uint32_t intr_status = PCNT.int_st.val;
	pcnt_evt_t evt;
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
    
	esp_log_write(ESP_LOG_DEBUG, TAG, "Common revolution detected");

	if (intr_status & (BIT(PCNT_UNIT))) {
		esp_log_write(ESP_LOG_DEBUG, TAG, "Revolution detected");

		state = !state;
		gpio_set_level(LED_BUILTIN, state);
        
		struct timeval now;
	    gettimeofday(&now, NULL);
		evt.time = now;
		evt.status = PCNT.status_unit[PCNT_UNIT].val;

		PCNT.int_clr.val = BIT(PCNT_UNIT);
		xQueueSendFromISR(pcnt_evt_queue, &evt, &xHigherPriorityTaskWoken);

		if (evt.status & PCNT_STATUS_THRES0_M) {
			pcnt_counter_clear(PCNT_UNIT);
		}

		if (xHigherPriorityTaskWoken == pdTRUE) {
			portYIELD_FROM_ISR();
		}
	}
}


void initPcnt(void)
{
	gpio_set_level(LED_BUILTIN, state);

	pcnt_evt_queue = xQueueCreate(REVOLUTION_MSG_QUEUE_SIZE, sizeof(pcnt_evt_t));
	if (pcnt_evt_queue == NULL) {
		esp_log_write(ESP_LOG_ERROR, TAG, "Error creating revolution message queue");
	}
	
	// gpio_pullup_dis(REED_PIN);
	// gpio_pulldown_en(REED_PIN);

	pcnt_config_t pcnt_config;
	pcnt_config.pulse_gpio_num = REED_PIN;
	pcnt_config.ctrl_gpio_num = PCNT_PIN_NOT_USED;
	pcnt_config.channel = PCNT_CHANNEL_0;
	pcnt_config.unit = PCNT_UNIT;
	// What to do on the positive / negative edge of pulse input?
	pcnt_config.pos_mode = PCNT_COUNT_INC;      // Count up on the positive edge
	pcnt_config.neg_mode = PCNT_COUNT_DIS;      // Keep the counter value on the negative edge
											    // What to do when control input is low or high?
	pcnt_config.lctrl_mode = PCNT_MODE_KEEP;    // Keep the primary counter mode if high
	pcnt_config.hctrl_mode = PCNT_MODE_KEEP;    // Keep the primary counter mode if high
												
	pcnt_config.counter_h_lim = PCNT_H_LIM_VAL; // Set the maximum limit values to watch
	pcnt_config.counter_l_lim = PCNT_L_LIM_VAL; // Set the minimum limit values to watch

	/* Initialize PCNT unit */
	pcnt_unit_config(&pcnt_config);

	/* Configure and enable the input filter */
	// pcnt_set_filter_value(PCNT_UNIT, 100);
	// pcnt_filter_enable(PCNT_UNIT);

	/* Set threshold 0 and 1 values and enable events to watch */
	pcnt_set_event_value(PCNT_UNIT, PCNT_EVT_THRES_1, PCNT_THRESH1_VAL);
	pcnt_event_enable(PCNT_UNIT, PCNT_EVT_THRES_1);
	pcnt_set_event_value(PCNT_UNIT, PCNT_EVT_THRES_0, PCNT_THRESH0_VAL);
	pcnt_event_enable(PCNT_UNIT, PCNT_EVT_THRES_0);
	/* Enable events on zero, maximum and minimum limit values */
	pcnt_event_enable(PCNT_UNIT, PCNT_EVT_ZERO);
	pcnt_event_enable(PCNT_UNIT, PCNT_EVT_H_LIM);
	pcnt_event_enable(PCNT_UNIT, PCNT_EVT_L_LIM);

	/* Initialize PCNT's counter */
	pcnt_counter_pause(PCNT_UNIT);
	pcnt_counter_clear(PCNT_UNIT);

	/* Register ISR handler and enable interrupts for PCNT unit */
	pcnt_isr_register(handleReedInterrupt, NULL, 0, NULL);
	pcnt_intr_enable(PCNT_UNIT);

	/* Everything is set up, now go to counting */
	pcnt_counter_resume(PCNT_UNIT);
	esp_log_write(ESP_LOG_INFO, TAG, "PCNT started.");
}

int16_t getNumberOfRevolutions(void) {
	int16_t pcntCntr = 0;
	if (pcnt_get_counter_value(PCNT_UNIT, &pcntCntr) != ESP_OK) {
		esp_log_write(ESP_LOG_ERROR, TAG, "Cannot read PCNT");
	}
	return pcntCntr;
}	
