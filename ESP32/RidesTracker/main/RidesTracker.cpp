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
#include "altimeter.h"
#include "wifi.hpp"
#include "https.h"
#include "Notifier.hpp"
#include "Button.hpp"

#include "Arduino.h"
#include <HTTPClient.h>

#include "record.hpp"
#include "time.hpp"
#include "bluetooth.hpp"

// #define WLAN_SSID       "NAHATCH"
// #define WLAN_PASS       "nahatch123"

#define WLAN_SSID       "sde-guest"
#define WLAN_PASS       "4Our6uest"

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
static uint32_t presure = 0;
static int batteryVoltage = 0;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

#define MAX_WIFI_CONNECTION_ATTEMPTS 3


enum upload_status_t {
    IDLE,
    STARTED,
    RUNNING,
    FINISHED
}

upload_status = IDLE;

SemaphoreHandle_t xSemaphore = NULL;

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
    esp_sleep_enable_timer_wakeup(WAKE_UP_TIME_SEC * 1000000);

    const uint64_t ext_wakeup_pin_1_mask = 1ULL << REED_PIN;
    const uint64_t ext_wakeup_pin_2_mask = 1ULL << BUTTON_PIN;


    ESP_LOGI( TAG, "Enabling EXT1 wakeup on pins GPIO%d\n", REED_PIN);
    ESP_LOGI( TAG, "Enabling EXT1 wakeup on pins GPIO%d\n", BUTTON_PIN);
    esp_sleep_enable_ext1_wakeup(ext_wakeup_pin_1_mask | ext_wakeup_pin_2_mask , ESP_EXT1_WAKEUP_ANY_HIGH);

    gpio_pullup_dis(REED_PIN);
	gpio_pulldown_en(REED_PIN);

    gpio_pullup_dis(BUTTON_PIN);
	gpio_pulldown_en(BUTTON_PIN);

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

void setLastKnownTime() {
    preferences.begin("RidezTracker", false);
	unsigned int resetTimes = preferences.getUInt("resetTimes", 0);
	resetTimes++;
	ESP_LOGI( TAG, "Number of restart times: %d\n", resetTimes);
	preferences.putUInt("resetTimes", resetTimes);

    struct timeval now = kalfy::time::getCurrentTime();
    struct timeval delta = kalfy::time::sub(&now, &sleep_enter_time);
    printf("Time spent in deep sleep: %ld ms\n", kalfy::time::toMiliSecs(&delta));
    
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

    lastActivityTime = kalfy::time::getCurrentTime();
}

wl_status_t connectWiFiManual() {
	// Connect to WiFi access point.
	Serial.println();
	Serial.println();
	
	Serial.print("Connecting to ");
	Serial.println(WLAN_SSID);

    //WiFi.mode(WIFI_OFF);
    delay(2000);
	Serial.println("Connecting Wifi ");
	for (int loops = MAX_WIFI_CONNECTION_ATTEMPTS; loops > 0; loops--) {
  
		//WiFi.disconnect(true);                                      // Clear Wifi Credentials
        //WiFi.persistent(false);                                     // Avoid to store Wifi configuration in Flash
        WiFi.mode(WIFI_STA);                                        // Ensure WiFi mode is Station 
    
		//WiFi.begin(WLAN_SSID, WLAN_PASS);
        WiFi.begin(connections[0].getSSID(), connections[0].getPassword());
		if (WiFi.status() == WL_CONNECTED) {
			Serial.println("");
			Serial.print("WiFi connected ");
			Serial.print("IP address: ");
			Serial.println(WiFi.localIP());
			break;
		}
		else {
			Serial.print("WiFi connection attempt: ");
			Serial.println(loops);
			
		}
		vTaskDelay(10000 / portTICK_PERIOD_MS);
		//delay(10000);
	}


	return WiFi.status();
}
void uploadTask(void *pvParameter)
{

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); 

    kalfy::record::uploadFile(ODOCYCLE_SERVER, ODOCYCLE_ID, ODOCYCLE_TOKEN, ODOCYCLE_CERT, kalfy::record::DESTINATION_FILE);

    vTaskDelay(2000 / portTICK_RATE_MS);

    vTaskDelete(NULL);   
}

void uploadTestTask(void *pvParameter)
{

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); 

    kalfy::record::uploadFile(ODOCYCLE_SERVER, ODOCYCLE_ID, ODOCYCLE_TOKEN, ODOCYCLE_CERT, kalfy::record::TEST_FILE);

    vTaskDelay(2000 / portTICK_RATE_MS);

    vTaskDelete(NULL);   
}

void periodicTask(void) {
    Serial.println("=== sendData called");
    kalfy::ble::run();
    Serial.println("sendData done");

    
    goToSleep();
}

void onDemandTask(void *pvParameter) {

   // connectWifi();
    connectWiFiManual();
    vTaskDelay(2000 / portTICK_RATE_MS);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
	printLocalTime();

    kalfy::record::createTestFile(kalfy::record::TEST_FILE);
	kalfy::record::uploadFile(ODOCYCLE_SERVER, ODOCYCLE_ID, ODOCYCLE_TOKEN, ODOCYCLE_CERT, kalfy::record::TEST_FILE);
	Serial.println("sendData done");
	disconnectWifi();
    upload_status = FINISHED;


    vTaskDelay(2000 / portTICK_RATE_MS);
    vTaskDelete(NULL);  
}


void getBatteryVoltage(int *batteryVoltage) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_CHANNEL,ADC_ATTEN_DB_0);
    *batteryVoltage = adc1_get_raw(ADC_CHANNEL);   
}


void printStatus(const RevolutionsCounter & revolutionsCounter, const int & batteryVoltage, const struct timeval & lastActivityTime, const struct timeval & delta) {
    ESP_LOGI( TAG, "Number of revolutions %d\n", (int)(revolutionsCounter.getNumberOfRevolutions()));
    ESP_LOGI( TAG, "Battery voltage %fV\n", 1.05*(122.0/22.0)*batteryVoltage/4096);
    ESP_LOGI(TAG, "Last activity: %ld sec|%ld us\n", lastActivityTime.tv_sec, lastActivityTime.tv_usec);
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
			kalfy::record::saveRevolution(msg.time, kalfy::record::DESTINATION_FILE);	
			kalfy::record::savePressure(presure, kalfy::record::DESTINATION_FILE);

            lastActivityTime = msg.time;

            Notifier * notifier = static_cast<Notifier *>(pvParameter);
            notifier->toggle();
		}

	    vTaskDelay(100 / portTICK_PERIOD_MS); //wait for 100 ms
	}
}

void reedTask(void) {
    RevolutionsCounter revolutionsCounter(REED_PIN); 
    Button button(BUTTON_PIN);
    Notifier notifier(LED_PIN);

    xTaskCreate(saveRotationTask, "saveRotationTask", 4096, &notifier, 10, NULL);  
    unsigned long printTime = 0;

    while (1) {

            if ( upload_status == IDLE && button.getValue() == true) {
                //kalfy::record::clear(kalfy::record::DESTINATION_FILE);
                kalfy::record::printAll(kalfy::record::DESTINATION_FILE);
                
                upload_status = STARTED;
            }

            switch (upload_status) {
                case STARTED: {
                    notifier.setHigh();
                    revolutionsCounter.disable();
                    upload_status = RUNNING;
                    xTaskCreate(&onDemandTask, "onDemand", 8192, NULL, 6, NULL);
                    break;
                }
                case RUNNING: {
                    lastActivityTime = kalfy::time::getCurrentTime();
                    break;
                }
                case FINISHED: {
                    notifier.setLow();
                    revolutionsCounter.enable(); 
                    upload_status = IDLE;
                    break;
                }
                default: {;}
            }
                
                
               // kalfy::record::printAll();
    
              //  initialise_wifi();
              //  xTaskCreate(&uploadTask, "uploadTask", 8192, NULL, 6, NULL);
            

            struct timeval now = kalfy::time::getCurrentTime();
            struct timeval delta = kalfy::time::sub(&now, &lastActivityTime);
           
            if (millis() - printTime > 5000  && upload_status != RUNNING) {
                printTime = millis();

                initAltimeter();
                getAltitude(&presure);
                getBatteryVoltage(&batteryVoltage);
                printStatus(revolutionsCounter, batteryVoltage, lastActivityTime, delta); 
            }

            if (kalfy::time::toMicroSecs(&delta) > MAX_IDLE_TIME_US) {
                revolutionsCounter.disable();
                goToSleep();
            }
        
        vTaskDelay(100 / portTICK_RATE_MS);
    }

}

void defaultTask(void) {
    reedTask();
}


void executeStartupMode(void) {

    switch (esp_sleep_get_wakeup_cause()) {
        case ESP_SLEEP_WAKEUP_EXT1: {
            uint64_t wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();
            if (wakeup_pin_mask != 0) {
                int pin = __builtin_ffsll(wakeup_pin_mask) - 1;
                printf("--- Wake up from GPIO %d\n", pin);
                switch (pin) {
                    case BUTTON_PIN: {
                        onDemandTask(NULL);
                        break; 
                    }
                    case REED_PIN: {
                       reedTask(); 
                       break; 
                    }           
                    default: {
                        defaultTask();
                    }    
                }    
            } else {
                printf("--- Wake up from GPIO\n");
                defaultTask();
            }
            break;
        }
        case ESP_SLEEP_WAKEUP_TIMER: {
            printf("--- Wake up from timer.\n");
            periodicTask();
            break;
        }
        default: {
            printf("--- Not a deep sleep restart!\n");
            defaultTask();
        }
    }
}

void app_main()
{
    Serial.begin(115200);

    initArduino();
    nvs_flash_init();
    setLastKnownTime();
    executeStartupMode();    
}



#ifdef __cplusplus
}
#endif
