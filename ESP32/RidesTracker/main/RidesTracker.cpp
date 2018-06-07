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
#include "files.hpp"
#include "time.hpp"
#include "bluetooth.hpp"



#define WAKE_UP_TIME_SEC (60*5)
#define TIMEZONE_DIFF_GMT_PRAGUE_MINS 60
#define DAYLIGHT_SAVING_MINS 60

#define ADC_CHANNEL ADC1_CHANNEL_0 /*!< ADC1 channel 0 is GPIO36 = VP pin*/

const gpio_num_t WIFI_BUTTON_PIN = GPIO_NUM_14;
const gpio_num_t BLE_BUTTON_PIN = GPIO_NUM_26;
const gpio_num_t REED_PIN = GPIO_NUM_13;
const gpio_num_t LED_PIN = GPIO_NUM_2;

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

void setLastKnownTime() {
    preferences.begin("RidezTracker", false);
	unsigned int resetTimes = preferences.getUInt("resetTimes", 0);
	resetTimes++;
	ESP_LOGI( TAG, "Number of restart times: %d\n", resetTimes);
	preferences.putUInt("resetTimes", resetTimes);

    struct timeval now = kalfy::time::getCurrentTime();
    struct timeval delta = kalfy::time::sub(&now, &sleep_enter_time);
    ESP_LOGI(TAG, "Time spent in deep sleep: %ld ms\n", kalfy::time::toMiliSecs(&delta));
    
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
    Notifier notifier(LED_PIN);
    kalfy::files::initSPIFFS();
    nvs_flash_init();

    notifier.setHigh();
    kalfy::record::printAll(kalfy::record::DESTINATION_FILE);
    ESP_LOGI(TAG, "=== sendData called");
    kalfy::BLE BLEupdater(kalfy::record::DESTINATION_FILE);
    BLEupdater.run();
    ESP_LOGI(TAG, "sendData done");
    notifier.setLow();
    
    goToSleep();
    
}

void getBatteryVoltage(float *batteryVoltage) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_CHANNEL,ADC_ATTEN_DB_0);
    *batteryVoltage = 1.05*(122.0/22.0)*adc1_get_raw(ADC_CHANNEL)/4096;
}


const char* THINGSBOARD_SERVER = "http://demo.thingsboard.io/api/v1/WLN5UNEzcFUWA9qvgRnX/telemetry";
const char* THINGSBOARD_ID = "659b6670-600b-11e8-9588-c3b186e30863";
const char* THINGSBOARD_TOKEN = "WLN5UNEzcFUWA9qvgRnX";
const char* THINGSBOARD_CERT = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIEkjCCA3qgAwIBAgIQCgFBQgAAAVOFc2oLheynCDANBgkqhkiG9w0BAQsFADA/\n" \
"MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\n" \
"DkRTVCBSb290IENBIFgzMB4XDTE2MDMxNzE2NDA0NloXDTIxMDMxNzE2NDA0Nlow\n" \
"SjELMAkGA1UEBhMCVVMxFjAUBgNVBAoTDUxldCdzIEVuY3J5cHQxIzAhBgNVBAMT\n" \
"GkxldCdzIEVuY3J5cHQgQXV0aG9yaXR5IFgzMIIBIjANBgkqhkiG9w0BAQEFAAOC\n" \
"AQ8AMIIBCgKCAQEAnNMM8FrlLke3cl03g7NoYzDq1zUmGSXhvb418XCSL7e4S0EF\n" \
"q6meNQhY7LEqxGiHC6PjdeTm86dicbp5gWAf15Gan/PQeGdxyGkOlZHP/uaZ6WA8\n" \
"SMx+yk13EiSdRxta67nsHjcAHJyse6cF6s5K671B5TaYucv9bTyWaN8jKkKQDIZ0\n" \
"Z8h/pZq4UmEUEz9l6YKHy9v6Dlb2honzhT+Xhq+w3Brvaw2VFn3EK6BlspkENnWA\n" \
"a6xK8xuQSXgvopZPKiAlKQTGdMDQMc2PMTiVFrqoM7hD8bEfwzB/onkxEz0tNvjj\n" \
"/PIzark5McWvxI0NHWQWM6r6hCm21AvA2H3DkwIDAQABo4IBfTCCAXkwEgYDVR0T\n" \
"AQH/BAgwBgEB/wIBADAOBgNVHQ8BAf8EBAMCAYYwfwYIKwYBBQUHAQEEczBxMDIG\n" \
"CCsGAQUFBzABhiZodHRwOi8vaXNyZy50cnVzdGlkLm9jc3AuaWRlbnRydXN0LmNv\n" \
"bTA7BggrBgEFBQcwAoYvaHR0cDovL2FwcHMuaWRlbnRydXN0LmNvbS9yb290cy9k\n" \
"c3Ryb290Y2F4My5wN2MwHwYDVR0jBBgwFoAUxKexpHsscfrb4UuQdf/EFWCFiRAw\n" \
"VAYDVR0gBE0wSzAIBgZngQwBAgEwPwYLKwYBBAGC3xMBAQEwMDAuBggrBgEFBQcC\n" \
"ARYiaHR0cDovL2Nwcy5yb290LXgxLmxldHNlbmNyeXB0Lm9yZzA8BgNVHR8ENTAz\n" \
"MDGgL6AthitodHRwOi8vY3JsLmlkZW50cnVzdC5jb20vRFNUUk9PVENBWDNDUkwu\n" \
"Y3JsMB0GA1UdDgQWBBSoSmpjBH3duubRObemRWXv86jsoTANBgkqhkiG9w0BAQsF\n" \
"AAOCAQEA3TPXEfNjWDjdGBX7CVW+dla5cEilaUcne8IkCJLxWh9KEik3JHRRHGJo\n" \
"uM2VcGfl96S8TihRzZvoroed6ti6WqEBmtzw3Wodatg+VyOeph4EYpr/1wXKtx8/\n" \
"wApIvJSwtmVi4MFU5aMqrSDE6ea73Mj2tcMyo5jMd6jmeWUHK8so/joWUoHOUgwu\n" \
"X4Po1QYz+3dszkDqMp4fklxBwXRsW10KXzPMTZ+sOPAveyxindmjkW8lGy+QsRlG\n" \
"PfZ+G6Z6h7mjem0Y+iWlkYcV4PIWL1iwBi8saCbGS5jN2p8M+X+Q7UNKEkROb3N6\n" \
"KOqkqm57TH2H3eDJAkSnh6/DNFu0Qg==\n" \
"-----END CERTIFICATE-----\n";

typedef struct alive_report_item_t {
    struct timeval timestamp;
    uint32_t presure;
    float voltage;
} alive_report_item_t;

void reportAlive(alive_report_item_t &report) {
    HTTPClient http;
    http.begin(String(THINGSBOARD_SERVER)); //THINGSBOARD_CERT
    http.addHeader("Content-Type", "application/json");

    char timestamp[20];
    kalfy::time::toMiliSecsStr(&(report.timestamp), timestamp);
    timestamp[14] = '\0';
   
    String payload = "{";
    payload += "\"ts\":"; payload += timestamp; payload += ",";
    payload += "\"values\": {"; 
    payload += "\"battery\":"; payload += report.voltage ; payload += ",";
    payload += "\"presure\":"; payload += report.presure;
    payload += "}}";

    // Send payload
    char attributes[120];
    payload.toCharArray( attributes, 120);
    if (payload.length() > 120) {
        ESP_LOGE(TAG, "Max payload is 120 Bytes, %d requested", payload.length());
    }

    ESP_LOGI(TAG, "Server: %s", THINGSBOARD_SERVER);
    ESP_LOGI(TAG, "Payload: \n%s", payload.c_str());

    int httpResponseCode = http.sendRequest("POST", payload);
    if (httpResponseCode == 201)
    {
        ESP_LOGI(TAG, "== Data sent successfully");
    }
    else
    {
        ESP_LOGI(TAG, "== Sending to server failed");
        String response = http.getString();
        ESP_LOGI(TAG, "HTTP response code:");
        ESP_LOGI(TAG, "%d", httpResponseCode);
        ESP_LOGI(TAG, "HTTP response body:");
        ESP_LOGI(TAG, "%s", response);
    }
    http.end();
    ESP_LOGI(TAG, "== Upload complete");
}

void updateTime(void) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    printLocalTime();
    preferences.begin("RidezTracker", false);
    preferences.putBytes("time_when_sleep", &sleep_enter_time, sizeof(sleep_enter_time));
    preferences.end();
}

void getAliveData(alive_report_item_t & report) {
    initAltimeter();
    vTaskDelay(2000 / portTICK_RATE_MS);
    getAltitude(&(report.presure));
    getBatteryVoltage(&(report.voltage));
    report.timestamp = kalfy::time::getCurrentTime();
}



File root;
//these callbacks will be invoked to read and write data to sdcard
//and process response
//and showing progress 
int responsef(uint8_t *buffer, int len){
  Serial.printf("%s\n", buffer);
  return 0;
}

int rdataf(uint8_t *buffer, int len){
  //read file to upload
  if (root.available()) {
    return root.read(buffer, len);
  }
  return 0;
}

int wdataf(uint8_t *buffer, int len){
  //write downloaded data to file
  return root.write(buffer, len);
}

void progressf(int percent){
  Serial.printf("%d\n", percent);
}

void WifiTask(void *pvParameter) {
  // put your setup code here, to run once:
  Serial.begin(115200);
    Notifier notifier(LED_PIN);

    kalfy::files::initSPIFFS();
    nvs_flash_init();

    vTaskDelay(1000 / portTICK_RATE_MS);

    notifier.setHigh();
    // if (connectWifiManual() ) {
        
    //     updateTime();    

    //     vTaskDelay(2000 / portTICK_RATE_MS);
    //     kalfy::record::createTestFile(kalfy::record::TEST_FILE);
    //     kalfy::record::printAll(kalfy::record::TEST_FILE);

    //     UDHttp udh;
    //     root = SD.open("test.pdf", FILE_WRITE);

    //     udh.upload("http://192.168.1.107:80/upload/upload.php", "test.pdf", root.size(), rdataf, progressf, responsef);
    //     root.close();
    //     Serial.printf("done uploading\n");

    //     disconnectWifi();
    //     upload_status = TASK_FINISHED;

    //     vTaskDelay(2000 / portTICK_RATE_MS);
    // }
}

void BLETask(void *pvParameter) {
    Notifier notifier(LED_PIN);

    kalfy::files::initSPIFFS();
    nvs_flash_init();

    vTaskDelay(1000 / portTICK_RATE_MS);

    notifier.setHigh();
    // if (connectWifiManual() ) {
        
    //     updateTime();    

    //     vTaskDelay(2000 / portTICK_RATE_MS);
    //     kalfy::record::printAll(kalfy::record::DESTINATION_FILE);

    //     alive_report_item_t report = {{0L,0L}, 0, 0.0};
    //     getAliveData(report);
    //     reportAlive(report);

    //     kalfy::record::uploadFile(ODOCYCLE_SERVER, ODOCYCLE_ID, ODOCYCLE_TOKEN, ODOCYCLE_CERT, kalfy::record::TESDESTINATION_FILET_FILE);
    //     disconnectWifi();
    //     upload_status = TASK_FINISHED;

    //     vTaskDelay(2000 / portTICK_RATE_MS);
    // }
    //kalfy::record::clear(kalfy::record::DESTINATION_FILE);
    kalfy::record::printAll(kalfy::record::DESTINATION_FILE);

    ESP_LOGI(TAG, "=== sendData called");
    kalfy::BLE BLEupdater(kalfy::record::DESTINATION_FILE);
    BLEupdater.run();
    ESP_LOGI(TAG, "sendData done");
    notifier.setLow();

    kalfy::files::detachSPIFFS();
    notifier.setLow();
    goToSleep();

    vTaskDelete(NULL);  
}


void testTask(void *pvParameter) {
    Notifier notifier(LED_PIN);

    kalfy::files::initSPIFFS();
    nvs_flash_init();

    vTaskDelay(1000 / portTICK_RATE_MS);

    kalfy::record::printAll(kalfy::record::TEST_FILE);

    notifier.setHigh();
    if (connectWifiManual() ) {
        
        updateTime();    

        vTaskDelay(2000 / portTICK_RATE_MS);
        kalfy::record::createTestFile(kalfy::record::TEST_FILE);
        kalfy::record::printAll(kalfy::record::TEST_FILE);

        alive_report_item_t report = {{0L,0L}, 0, 0.0};
        getAliveData(report);
        reportAlive(report);

        kalfy::record::uploadMultipartFile(ODOCYCLE_SERVER, ODOCYCLE_ID, ODOCYCLE_TOKEN, ODOCYCLE_CERT, kalfy::record::TEST_FILE);
        disconnectWifi();

        vTaskDelay(2000 / portTICK_RATE_MS);
    }

    // kalfy::record::createTestFile(kalfy::record::TEST_FILE);
    // kalfy::record::printAll(kalfy::record::TEST_FILE);

    // ESP_LOGI(TAG, "=== sendData called");
    // kalfy::BLE BLEupdater(kalfy::record::TEST_FILE);
    // BLEupdater.run();
    // ESP_LOGI(TAG, "sendData done");
    // notifier.setLow();

    kalfy::files::detachSPIFFS();
    notifier.setLow();
    goToSleep();

    vTaskDelete(NULL);  
}





void printStatus(const RevolutionsCounter & revolutionsCounter, const float & batteryVoltage, const struct timeval & lastActivityTime, const struct timeval & delta) {
    ESP_LOGI( TAG, "Number of revolutions %d\n", (int)(revolutionsCounter.getNumberOfRevolutions()));
    ESP_LOGI( TAG, "Battery voltage %fV\n", batteryVoltage);
    time_t t = lastActivityTime.tv_sec;
	ESP_LOGI( TAG, "Last activity: %s", ctime(&t));
   // ESP_LOGI( TAG, "Last activity: %ld sec|%ld us\n", lastActivityTime.tv_sec, lastActivityTime.tv_usec);
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
    Button WifiButton(WIFI_BUTTON_PIN);
    Button BLEButton(BLE_BUTTON_PIN);
    Notifier notifier(LED_PIN);
    kalfy::files::initSPIFFS();

    xTaskCreate(saveRotationTask, "saveRotationTask", 4096, &notifier, 10, NULL);  
    unsigned long printTime = 0;

    while (1) {

            switch (upload_status) {
                case WIFI_STARTED: {
                    notifier.setHigh();
                    revolutionsCounter.disable();
                    upload_status = TASK_RUNNING;
                    xTaskCreate(&testTask, "onDemand", 20000, NULL, 6, NULL);
                    break;
                }
                case BLE_STARTED: {
                    notifier.setHigh();
                    revolutionsCounter.disable();
                    upload_status = TASK_RUNNING;
                    xTaskCreate(&BLETask, "onDemand", 8192, NULL, 6, NULL);
                    break;
                }
                case TASK_RUNNING: {
                    lastActivityTime = kalfy::time::getCurrentTime();
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
                
            struct timeval now = kalfy::time::getCurrentTime();
            struct timeval delta = kalfy::time::sub(&now, &lastActivityTime);
           
            if (millis() - printTime > 5000  && upload_status != TASK_RUNNING) {
                printTime = millis();

                initAltimeter();
                getAltitude(&presure);
                getBatteryVoltage(&batteryVoltage);
                printStatus(revolutionsCounter, batteryVoltage, lastActivityTime, delta); 
            }

            if (kalfy::time::toMicroSecs(&delta) > MAX_IDLE_TIME_US) {
                revolutionsCounter.disable();
                kalfy::files::detachSPIFFS();
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
                    case WIFI_BUTTON_PIN: {
                        testTask(NULL);
                        break; 
                    }
                    case BLE_BUTTON_PIN: {
                        BLETask(NULL);
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
