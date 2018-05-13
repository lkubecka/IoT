/* Deep sleep wake up example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <driver/adc.h>
#include <driver/gpio.h>
#include "esp_sleep.h"
#include "esp32/ulp.h"
#include "driver/touch_pad.h"
#include "driver/adc.h"
#include "driver/rtc_io.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "soc/rtc.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "bmp180.h"
#include "revolutions_counter.hpp"
#include "Preferences.hpp"
#include "time.hpp"
#include "FileSystem.hpp"
#include "File.hpp"
#include "altimeter.h"
#include "wifi.hpp"
#include "https.h"

#define WAKE_UP_TIME_SEC 60
#define TIMEZONE_DIFF_GMT_PRAGUE_MINS 60
#define DAYLIGHT_SAVING_MINS 60

#define ADC_CHANNEL ADC1_CHANNEL_0 /*!< ADC1 channel 0 is GPIO36 = VP pin*/

const gpio_num_t BUTTON_PIN = GPIO_NUM_14;
const gpio_num_t REED_PIN = GPIO_NUM_13;
const gpio_num_t LED_PIN = GPIO_NUM_2;

static const char* TAG = "RideTracker";
static RTC_DATA_ATTR struct timeval sleep_enter_time;
static struct timeval lastActivityTime = { 0UL, 0UL };
const uint64_t MAX_IDLE_TIME_US = 60 * 1000 * 1000; // 1 minute IDLE
volatile uint8_t state = 0x00;
Preferences preferences;

#ifdef __cplusplus
extern "C" {
#endif

void goToSleep(void) {
    esp_log_write(ESP_LOG_INFO, TAG, "Going to sleep\n");
    esp_log_write(ESP_LOG_INFO, TAG, "Enabling timer wakeup, %ds\n", WAKE_UP_TIME_SEC);
    esp_sleep_enable_timer_wakeup(WAKE_UP_TIME_SEC * 1000000);

    const uint64_t ext_wakeup_pin_1_mask = 1ULL << REED_PIN;
    const uint64_t ext_wakeup_pin_2_mask = 1ULL << BUTTON_PIN;


    esp_log_write(ESP_LOG_INFO, TAG, "Enabling EXT1 wakeup on pins GPIO%d\n", REED_PIN);
    esp_log_write(ESP_LOG_INFO, TAG, "Enabling EXT1 wakeup on pins GPIO%d\n", BUTTON_PIN);
    esp_sleep_enable_ext1_wakeup(ext_wakeup_pin_1_mask | ext_wakeup_pin_2_mask , ESP_EXT1_WAKEUP_ANY_HIGH);

    gpio_pullup_dis(REED_PIN);
	gpio_pulldown_en(REED_PIN);

    gpio_pullup_dis(BUTTON_PIN);
	gpio_pulldown_en(BUTTON_PIN);

    // Isolate GPIO12 pin from external circuits. This is needed for modules
    // which have an external pull-up resistor on GPIO12 (such as ESP32-WROVER)
    // to minimize current consumption.
    rtc_gpio_isolate(GPIO_NUM_12);

    esp_log_write(ESP_LOG_INFO, TAG, "Entering deep sleep\n");
    gettimeofday(&sleep_enter_time, NULL);

	preferences.begin("RidezTracker", false);
	preferences.putBytes("time_when_sleep", &sleep_enter_time, sizeof(sleep_enter_time));
	preferences.end();

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    esp_deep_sleep_start();
}

void setLastKnownTime() {
    preferences.begin("RidezTracker", false);
	unsigned int resetTimes = preferences.getUInt("resetTimes", 0);
	resetTimes++;
	esp_log_write(ESP_LOG_INFO, TAG, "Number of restart times: %d\n", resetTimes);
	preferences.putUInt("resetTimes", resetTimes);

    struct timeval now = kalfy::time::getCurrentTime();
    struct timeval delta = kalfy::time::sub(&now, &sleep_enter_time);
    esp_log_write(ESP_LOG_INFO, TAG, "Time spent in deep sleep: %ld ms\n", kalfy::time::toMiliSecs(&delta));
    
    time_t t = time(NULL);
    esp_log_write(ESP_LOG_INFO, TAG, "Current date/time: %s", ctime(&t));
	if (now.tv_sec < 10000) {
		
		preferences.getBytes("time_when_sleep", &now, sizeof(now));
		esp_log_write(ESP_LOG_INFO, TAG, "Time lost, loading last known time: %ld\n", now.tv_sec);
		struct timezone tz;
		tz.tz_minuteswest = TIMEZONE_DIFF_GMT_PRAGUE_MINS;
		tz.tz_dsttime = DAYLIGHT_SAVING_MINS;
		settimeofday(&now, &tz);
		t = time(NULL);
		esp_log_write(ESP_LOG_INFO, TAG, "Current date/time: %s", ctime(&t));
	}

	preferences.end();

    lastActivityTime = kalfy::time::getCurrentTime();
}

void selectStartupMode(void) {

    switch (esp_sleep_get_wakeup_cause()) {
        case ESP_SLEEP_WAKEUP_EXT1: {
            uint64_t wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();
            if (wakeup_pin_mask != 0) {
                int pin = __builtin_ffsll(wakeup_pin_mask) - 1;
                printf("--- Wake up from GPIO %d\n", pin);
            } else {
                printf("--- Wake up from GPIO\n");
            }
            break;
        }
        case ESP_SLEEP_WAKEUP_TIMER: {
            printf("--- Wake up from timer.\n");
            break;
        }
        default: {
            printf("--- Not a deep sleep restart!\n");
        }
    }
}

void saveRotationTask(void *pvParameter)
{
	RevolutionsCounter::pcnt_evt_t msg;
	BaseType_t xStatus;

	if (counterEventQueue == NULL) {
		esp_log_write(ESP_LOG_ERROR, TAG, "Revolution event queue not ready.");
		return;
	}

    File recordFile("/spiffs/records.txt");
    
	while (1) {
		xStatus = xQueueReceive(counterEventQueue, &msg, 0);
		if (xStatus == pdPASS) {
            esp_log_write(ESP_LOG_INFO, TAG, "t%lu|%lu\n", (time_t)msg.time.tv_sec, (suseconds_t)msg.time.tv_usec);
	        gpio_set_level(LED_PIN, state = !state);

            char buffer[32];
			snprintf(buffer, sizeof(buffer), "t%ld|%ld", msg.time.tv_sec, msg.time.tv_usec);
            recordFile.appendToFile(buffer);
			//kalfy::record::saveRevolution(msg.time);	
			//kalfy::record::savePressure(presure);

            lastActivityTime = msg.time;
		}

	    vTaskDelay(100 / portTICK_PERIOD_MS); //wait for 100 ms
	}
}

void app_main()
{

    nvs_flash_init();

    FileSystem fileSystem;
    fileSystem.begin();
    fileSystem.info();


    setLastKnownTime();

    selectStartupMode();
    
    RevolutionsCounter RevolutionsCounter(REED_PIN); 

    xTaskCreate(
		saveRotationTask,       /* Task function. */
		"ReedConsumer",         /* String with name of task. */
		4096,                   /* Stack size in words. */
		NULL,                   /* Parameter passed as input of the task */
		10,                     /* Priority of the task. */
		NULL);                  /* Task handle. */

    //xTaskCreatePinnedToCore(bmp180_test, "bmp180_test", configMINIMAL_STACK_SIZE * 15, NULL, 5, NULL, APP_CPU_NUM);
   // xTaskCreate(bmp180_test, "bmp180_test", configMINIMAL_STACK_SIZE * 15, NULL, 5, NULL);

    // first time update of reference pressure
    File recordFile("/spiffs/records.txt");


    gpio_pullup_dis(BUTTON_PIN);
	gpio_pulldown_en(BUTTON_PIN);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);

    

    while (1) {

            printf("GPIO[%d] val: %d\n", BUTTON_PIN, gpio_get_level(BUTTON_PIN));
            if ( gpio_get_level(BUTTON_PIN) == true) {
                RevolutionsCounter.disable();
				gpio_intr_disable(BUTTON_PIN);
                gpio_set_level(LED_PIN, state = true);
                
                recordFile.printFile();
                initialise_wifi();

                xTaskCreate(&https_request_task, "https_request_task", 8192, NULL, 6, NULL);
                //https_request_task(NULL);
                //recordFile.deleteFile();
                
                gpio_set_level(LED_PIN, state = false);
                gpio_intr_enable(BUTTON_PIN);
                RevolutionsCounter.enable();
            }

            esp_log_write(ESP_LOG_INFO, TAG, "Number of revolutions %d\n", RevolutionsCounter.getNumberOfRevolutions());
            
            initAltimeter();
            getAltitude();

            adc1_config_width(ADC_WIDTH_BIT_12);
            adc1_config_channel_atten(ADC_CHANNEL,ADC_ATTEN_DB_0);
            int val = adc1_get_raw(ADC_CHANNEL);    

            esp_log_write(ESP_LOG_INFO, TAG, "Battery voltage %dV\n", val);
            esp_log_write(ESP_LOG_INFO, TAG, "Battery voltage %fV\n", 1.05*(122.0/22.0)*val/4096);
           

			esp_log_write(ESP_LOG_INFO, TAG, "\n\nLast activity: %ld sec|%ld us\n", lastActivityTime.tv_sec, lastActivityTime.tv_usec);
			
            struct timeval now = kalfy::time::getCurrentTime();
			struct timeval delta = kalfy::time::sub(&now, &lastActivityTime);
			esp_log_write(ESP_LOG_INFO, TAG,"Time since last activity:  %ld sec|%ld us\n", delta.tv_sec, delta.tv_usec);
            
            // if (kalfy::time::toMicroSecs(&delta) > (10*1000*1000)) {
            //     recordFile.printFile();
            // }

            if (kalfy::time::toMicroSecs(&delta) > MAX_IDLE_TIME_US) {
                RevolutionsCounter.disable();

				gpio_intr_disable(BUTTON_PIN);
                fileSystem.end();
				goToSleep();
			}
        
        vTaskDelay(5000 / portTICK_RATE_MS);
    }
    
}



#ifdef __cplusplus
}
#endif
