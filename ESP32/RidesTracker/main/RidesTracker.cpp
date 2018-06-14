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
#include "altimeter.h"
#include "wifi.hpp"
#include "https.h"
#include "Notifier.hpp"
#include "Button.hpp"

#include "Arduino.h"
#include "record.hpp"
#include "files.hpp"
#include "time.hpp"
#include "bluetooth.hpp"
#include "thingsboardPoster.hpp"


const uint64_t uS_TO_S_FACTOR = 1000000;
const uint64_t mS_TO_S_FACTOR = 1000;
const uint64_t WAKE_UP_TIME_SEC = (1*60);  // once in 1 minutes
const uint64_t POST_DATA_PERIOD_SEC = (1*60*60);  // once in 1 hours

#define TIMEZONE_DIFF_GMT_PRAGUE_MINS 60
#define DAYLIGHT_SAVING_MINS 60

#define ADC_CHANNEL ADC1_CHANNEL_0 /*!< ADC1 channel 0 is GPIO36 = VP pin*/

const gpio_num_t WIFI_BUTTON_PIN = GPIO_NUM_14;
const gpio_num_t BLE_BUTTON_PIN = GPIO_NUM_26;
const gpio_num_t REED_PIN = GPIO_NUM_13;
const gpio_num_t LED_PIN = GPIO_NUM_2;

const int MAX_STACK_SIZE = 8192;

static const char* TAG = "RideTracker";
static RTC_DATA_ATTR struct timeval sleep_enter_time;
static struct timeval lastActivityTime = { 0UL, 0UL };
const uint64_t MAX_IDLE_TIME_US = 60 * 1000 * 1000; // 1 minute IDLE
volatile uint8_t state = 0x00;
Preferences preferences;
static uint32_t presure = 0;
static float batteryVoltage = 0;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

enum upload_status_t {
    IDLE,
    WIFI_STARTED,
    BLE_STARTED,
    TASK_RUNNING,
    TASK_FINISHED
}

upload_status = IDLE;

SemaphoreHandle_t xSemaphore = NULL;

using namespace kalfy::time;
using namespace kalfy::record;
using namespace kalfy::files;

#ifdef __cplusplus
extern "C" {
#endif

void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void goToSleep(void) {
    ESP_LOGI( TAG, "Going to sleep\n");
    ESP_LOGI( TAG, "Enabling timer wakeup, %ds\n", WAKE_UP_TIME_SEC);
    esp_sleep_enable_timer_wakeup(WAKE_UP_TIME_SEC * uS_TO_S_FACTOR);

    const uint64_t reed_mask = 1ULL << REED_PIN;
    const uint64_t wifi_mask = 1ULL << WIFI_BUTTON_PIN;
    const uint64_t ble_mask = 1ULL << BLE_BUTTON_PIN;


    ESP_LOGI( TAG, "Enabling EXT1 wakeup on pins GPIO%d\n", REED_PIN);
    ESP_LOGI( TAG, "Enabling EXT1 wakeup on pins GPIO%d\n", WIFI_BUTTON_PIN);
    ESP_LOGI( TAG, "Enabling EXT1 wakeup on pins GPIO%d\n", BLE_BUTTON_PIN);
    esp_sleep_enable_ext1_wakeup(reed_mask | wifi_mask | ble_mask , ESP_EXT1_WAKEUP_ANY_HIGH);

    gpio_pullup_dis(REED_PIN);
	gpio_pulldown_en(REED_PIN);

    gpio_pullup_dis(WIFI_BUTTON_PIN);
	gpio_pulldown_en(WIFI_BUTTON_PIN);

    gpio_pullup_dis(BLE_BUTTON_PIN);
	gpio_pulldown_en(BLE_BUTTON_PIN);

    // Isolate GPIO12 pin from external circuits. This is needed for modules
    // which have an external pull-up resistor on GPIO12 (such as ESP32-WROVER)
    // to minimize current consumption.
    rtc_gpio_isolate(GPIO_NUM_12);

    ESP_LOGI( TAG, "Entering deep sleep\n");
    gettimeofday(&sleep_enter_time, NULL);

	preferences.begin("RidezTracker", false);
	preferences.putBytes("time_when_sleep", &sleep_enter_time, sizeof(sleep_enter_time));
	preferences.end();

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    esp_deep_sleep_start();
}

void retreiveTimeWhenPost(struct timeval *time_when_post) {
    nvs_flash_init();
    vTaskDelay(1000 / portTICK_RATE_MS);
    preferences.begin("RidezTracker", false);
    preferences.getBytes("time_when_post", time_when_post, sizeof(*time_when_post));
    preferences.end();

}

bool isTimeToSendData() {
    struct timeval time_when_post;
    retreiveTimeWhenPost(&time_when_post);
    struct timeval now = getCurrentTime();
    struct timeval delta = sub(&now, &time_when_post);
    printTimeDifference(delta.tv_sec);

    return (delta.tv_sec > POST_DATA_PERIOD_SEC );
}

void setLastKnownTime() {
    nvs_flash_init();
    vTaskDelay(1000 / portTICK_RATE_MS);

    preferences.begin("RidezTracker", false);
	unsigned int resetTimes = preferences.getUInt("resetTimes", 0);
	resetTimes++;
	ESP_LOGI( TAG, "Number of restart times: %d\n", resetTimes);
	preferences.putUInt("resetTimes", resetTimes);

    struct timeval sleep_enter_time;
    preferences.getBytes("time_when_sleep", &sleep_enter_time, sizeof(sleep_enter_time));
    struct timeval now = getCurrentTime();
    struct timeval delta = sub(&now, &sleep_enter_time);
    ESP_LOGI(TAG, "Time spent in deep sleep: %ld ms\n", toMiliSecs(&delta));
    
    time_t t = time(NULL);
    printf("Current date/time: %s", ctime(&t));
	if (now.tv_sec < 10000) {
		
		preferences.getBytes("time_when_sleep", &now, sizeof(now));
		ESP_LOGI( TAG, "Time lost, loading last known time: %ld\n", now.tv_sec);
		struct timezone tz;
		tz.tz_minuteswest = TIMEZONE_DIFF_GMT_PRAGUE_MINS;
		tz.tz_dsttime = DAYLIGHT_SAVING_MINS;
		settimeofday(&now, &tz);
		t = time(NULL);
		ESP_LOGI( TAG, "Current date/time: %s", ctime(&t));
	}

	preferences.end();

    lastActivityTime = getCurrentTime();
}

void getBatteryVoltage(float *batteryVoltage) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_CHANNEL,ADC_ATTEN_DB_0);
    *batteryVoltage = 1.05*(122.0/22.0)*adc1_get_raw(ADC_CHANNEL)/4096;
}

void getAliveData(alive_report_item_t & report) {
    initAltimeter();
    vTaskDelay(2000 / portTICK_RATE_MS);
    getAltitude(&(report.presure));
    getBatteryVoltage(&(report.voltage));
    report.timestamp = getCurrentTime();
}

void updateTime(void) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    printLocalTime();
    preferences.begin("RidezTracker", false);
    preferences.putBytes("time_when_sleep", &sleep_enter_time, sizeof(sleep_enter_time));
    preferences.end();
}

void storeTimeWhenPost() {
    nvs_flash_init();
    vTaskDelay(1000 / portTICK_RATE_MS);
    struct timeval time_when_post;
    gettimeofday(&time_when_post, NULL);
    preferences.begin("RidezTracker", false);
    preferences.putBytes("time_when_post", &time_when_post, sizeof(time_when_post));
    preferences.end();
}


void periodicTask(void *pvParameter) {
    Notifier notifier(LED_PIN);

    nvs_flash_init();
    initSPIFFS();
    
    vTaskDelay(1000 / portTICK_RATE_MS);

    notifier.setHigh();
    if (connectWifiManual() ) {
        
        updateTime();    

        vTaskDelay(2000 / portTICK_RATE_MS);

        printAll(DESTINATION_FILE);

        alive_report_item_t report = {{0L,0L}, 0, 0.0};
        getAliveData(report);
        reportAlive(report);

        uploadMultipartFile(DESTINATION_FILE);
        storeTimeWhenPost();

        disconnectWifi();

        vTaskDelay(2000 / portTICK_RATE_MS);
    }
       
    goToSleep();
    
}

void BLETask(void *pvParameter) {
    Notifier notifier(LED_PIN);

    nvs_flash_init();
    initSPIFFS();
    vTaskDelay(1000 / portTICK_RATE_MS);

    notifier.setHigh();

    ESP_LOGI(TAG, "=== sendData called");
    kalfy::BLE BLEupdater(DESTINATION_FILE);
    BLEupdater.run();
    ESP_LOGI(TAG, "sendData done");
    notifier.setLow();

    detachSPIFFS();
    notifier.setLow();
    goToSleep();

    vTaskDelete(NULL);  
}


void testTask(void *pvParameter) {
    Notifier notifier(LED_PIN);

    nvs_flash_init();
    initSPIFFS();

    vTaskDelay(1000 / portTICK_RATE_MS);

    printAll(TEST_FILE);

    notifier.setHigh();
    if (connectWifiManual() ) {
        
        updateTime();    

        vTaskDelay(2000 / portTICK_RATE_MS);
        createTestFile(TEST_FILE);
        printAll(TEST_FILE);

        alive_report_item_t report = {{0L,0L}, 0, 0.0};
        getAliveData(report);
        reportAlive(report);

        uploadMultipartFile(TEST_FILE);
        disconnectWifi();

        vTaskDelay(2000 / portTICK_RATE_MS);
    }

    // createTestFile(TEST_FILE);
    // printAll(TEST_FILE);

    // ESP_LOGI(TAG, "=== sendData called");
    // kalfy::BLE BLEupdater(TEST_FILE);
    // BLEupdater.run();
    // ESP_LOGI(TAG, "sendData done");
    // notifier.setLow();

    detachSPIFFS();
    notifier.setLow();
    goToSleep();

    vTaskDelete(NULL);  
}

void printStatus(const RevolutionsCounter & revolutionsCounter, const float & batteryVoltage, const struct timeval & lastActivityTime, const struct timeval & delta) {
    ESP_LOGI( TAG, "Number of revolutions %d\n", (int)(revolutionsCounter.getNumberOfRevolutions()));
    ESP_LOGI( TAG, "Battery voltage %fV\n", batteryVoltage);
    time_t t = lastActivityTime.tv_sec;
	ESP_LOGI( TAG, "Last activity: %s", ctime(&t));
    ESP_LOGI( TAG,"Time since last activity:  %ld sec|%ld us\n", delta.tv_sec, delta.tv_usec);
}

void saveRotationTask(void *pvParameter)
{
	RevolutionsCounter::pcnt_evt_t msg;
	BaseType_t xStatus;

	if (counterEventQueue == NULL) {
		esp_log_write(ESP_LOG_ERROR, TAG, "Revolution event queue not ready.");
		return;
	}

	while (1) {
		xStatus = xQueueReceive(counterEventQueue, &msg, 0);
		if (xStatus == pdPASS) {
            ESP_LOGI( TAG, "t%lu|%lu\n", (time_t)msg.time.tv_sec, (suseconds_t)msg.time.tv_usec);
	        gpio_set_level(LED_PIN, state = !state);

            char buffer[32];
			snprintf(buffer, sizeof(buffer), "t%ld|%ld", msg.time.tv_sec, msg.time.tv_usec);
			saveRevolution(msg.time, DESTINATION_FILE);	
            if (presure > 1000) {
			    savePressure(presure, DESTINATION_FILE);
            }
            lastActivityTime = msg.time;

            Notifier * notifier = static_cast<Notifier *>(pvParameter);
            notifier->toggle();
		}

	    vTaskDelay(100 / portTICK_PERIOD_MS); //wait for 100 ms
	}
}

void reedTask(void *pvParameter) {
    RevolutionsCounter revolutionsCounter(REED_PIN); 
    Button WifiButton(WIFI_BUTTON_PIN);
    Button BLEButton(BLE_BUTTON_PIN);
    Notifier notifier(LED_PIN);
    initSPIFFS();

    xTaskCreate(saveRotationTask, "saveRotationTask", 4096, &notifier, 10, NULL);  
    unsigned long printTime = 0;

    while (1) {

            switch (upload_status) {
                case WIFI_STARTED: {
                    notifier.setHigh();
                    revolutionsCounter.disable();
                    upload_status = TASK_RUNNING;
                    xTaskCreate(&periodicTask, "onDemand", MAX_STACK_SIZE, NULL, 6, NULL);
                    break;
                }
                case BLE_STARTED: {
                    notifier.setHigh();
                    revolutionsCounter.disable();
                    upload_status = TASK_RUNNING;
                    xTaskCreate(&BLETask, "onDemand", MAX_STACK_SIZE, NULL, 6, NULL);
                    break;
                }
                case TASK_RUNNING: {
                    lastActivityTime = getCurrentTime();
                    break;
                }
                case TASK_FINISHED: {
                    notifier.setLow();
                    revolutionsCounter.enable(); 
                    upload_status = IDLE;
                    break;
                }
                default: {
                    if ( WifiButton.getValue() == true) {
                        upload_status = WIFI_STARTED;
                    }
                    if ( BLEButton.getValue() == true) {
                        
                        upload_status = BLE_STARTED;
                    }


                }
            }
                
            struct timeval now = getCurrentTime();
            struct timeval delta = sub(&now, &lastActivityTime);
           
            if (millis() - printTime > 5000  && upload_status != TASK_RUNNING) {
                printTime = millis();

                initAltimeter();
                getAltitude(&presure);
                getBatteryVoltage(&batteryVoltage);
                printStatus(revolutionsCounter, batteryVoltage, lastActivityTime, delta); 
            }

            if (toMicroSecs(&delta) > MAX_IDLE_TIME_US) {
                revolutionsCounter.disable();
                detachSPIFFS();
                goToSleep();
            }
        
        vTaskDelay(100 / portTICK_RATE_MS);
    }

}

void defaultTask(void *pvParameter) {
    reedTask(NULL);
}


void executeStartupMode(void) {

    switch (esp_sleep_get_wakeup_cause()) {
        case ESP_SLEEP_WAKEUP_EXT1: {
            uint64_t wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();
            if (wakeup_pin_mask != 0) {
                int pin = __builtin_ffsll(wakeup_pin_mask) - 1;
                printf("--- Wake up from GPIO %d\n", pin);
                switch (pin) {
                    case WIFI_BUTTON_PIN: {
                        xTaskCreate(&periodicTask, "WifiTask", MAX_STACK_SIZE, NULL, 6, NULL);
                        break; 
                    }
                    case BLE_BUTTON_PIN: {
                        xTaskCreate(&BLETask, "BleTask", MAX_STACK_SIZE, NULL, 6, NULL);
                        break; 
                    }
                    case REED_PIN: {
                        xTaskCreate(&reedTask, "reedTask", MAX_STACK_SIZE, NULL, 6, NULL);
                        break; 
                    }           
                    default: {
                        xTaskCreate(&defaultTask, "defaultTask", MAX_STACK_SIZE, NULL, 6, NULL);
                    }    
                }    
            } else {
                printf("--- Wake up from GPIO\n");
                xTaskCreate(&defaultTask, "defaultTask", MAX_STACK_SIZE, NULL, 6, NULL);
            }
            break;
        }
        case ESP_SLEEP_WAKEUP_TIMER: {
            printf("--- Wake up from timer.\n");
            if (isTimeToSendData()) {
                xTaskCreate(&periodicTask, "periodicTask", MAX_STACK_SIZE, NULL, 6, NULL);
            }
            break;
        }
        default: {
            printf("--- Not a deep sleep restart!\n");
            xTaskCreate(&defaultTask, "defaultTask", MAX_STACK_SIZE, NULL, 6, NULL);
        }
    }
}

void app_main()
{
    Serial.begin(115200);

    initArduino();
    setLastKnownTime();
    executeStartupMode(); 
    vTaskDelete(NULL);   
}



#ifdef __cplusplus
}
#endif
