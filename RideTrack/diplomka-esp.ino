#include <driver/rtc_io.h>
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <Preferences.h>

#include "freertos/FreeRTOS.h"
//#include "esp_wifi.h"
//#include "esp_system.h"
//#include "esp_event.h"
//#include "esp_event_loop.h"
//#include "nvs_flash.h"

#include <HTTPClient.h>
#include "record.hpp"
#include "time.hpp"

#include "driver/pcnt.h"
#include "esp_deep_sleep.h"
#include "ble.hpp"

#include "soc/rtc_cntl_reg.h"

#define _DEBUG

#define uS_TO_MS 1000
#define HAS_CONTACT 1
#define NO_CONTACT 0
volatile byte state = HIGH;

const int MAX_WIFI_CONNECTION_ATTEMPTS = 3;
const int MEASURE_PRESSURE_EVERY_NTH_ROTATION = 100;
const int RECORD_NTH_ROTATION = 10;
const uint64_t MIN_SAMPLING_TIME_US = 85UL;
const uint64_t MAX_IDLE_TIME_US = 600 * 1000 * 1000; // 10 minutes IDLE

const gpio_num_t LED_BUILTIN = GPIO_NUM_2;
const gpio_num_t REED_PIN = GPIO_NUM_13;
const gpio_num_t BUTTON_PIN = GPIO_NUM_14;
const uint32_t TRIGGERED_BY_REED = BIT(13);
const uint32_t TRIGGERED_BY_BUTTON = BIT(14);
const uint64_t SLEEP_INTERVAL_uS = 20 * uS_TO_MS;
const uint64_t ALTITUDE_UPDATE_PERIOD_MS = 20000;
const uint64_t LAST_ACTIVITY_UPDATE_PERIOD_MS = 100000;
const gpio_num_t BATTERY_PIN = GPIO_NUM_36; //VP pin

const uint64_t uS_TO_S_FACTOR = 1000000UL;  /* Conversion factor for micro seconds to seconds */
const uint64_t TIME_TO_SLEEP = 300UL;        /* Time ESP32 will go to sleep (in seconds) */

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;


const pcnt_unit_t PCNT_UNIT = PCNT_UNIT_0;
const int PCNT_H_LIM_VAL = 32767;
const int PCNT_L_LIM_VAL = -1;
const int PCNT_THRESH1_VAL = 200;
const int PCNT_THRESH0_VAL = RECORD_NTH_ROTATION;
const int PCNT_INPUT_SIG_IO = REED_PIN;


xQueueHandle pcnt_evt_queue;   // A queue to handle pulse counter events



/* A structure to pass events from the PCNTinterrupt handler to the main program.*/
typedef struct {
	timeval time;
	uint32_t status; // information on the event type that caused the interrupt
} pcnt_evt_t;


volatile unsigned long isrCounter = 0;
volatile unsigned long lastIsrAt = 0;

static uint64_t numberOfInterrupts = 0;

#define WLAN_SSID       "sde-guest"
#define WLAN_PASS       "4Our6uest"

//#define WLAN_SSID       "Redmi_Libor"
//#define WLAN_PASS       "nahatch123"

//#define WLAN_SSID       "NAHATCH"
//#define WLAN_PASS       "nahatch123"

const char* ODOCYCLE_SERVER = "https://ridestracker.azurewebsites.net/api/v1/Records/";
const char* ODOCYCLE_ID = "4f931a53-6e1b-4e85-bbda-7c71d9f8f2b9";
const char* ODOCYCLE_TOKEN = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiI5YmQwYzhmMC05ZTI0LTQxZTgtYjkwNi02ZDI3MGFjNGFkN2UiLCJqdGkiOiI0MjA2NmM1YS1lZTA5LTQwNmMtOGIzNC05ZGMyNzVmMjZmMDciLCJleHAiOjE1Mjg0Njk0MTYsImlzcyI6ImF6dXJlLWRldiIsImF1ZCI6ImF6dXJlLWRldiJ9.7-TDYGT48sX0VksCpAYzYCAtXuJ2Djoeq2HCIcEFsGY";

const char* ODOCYCLE_CERT = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIFtDCCBJygAwIBAgIQC2qzsD6xqfbEYJJqqM3+szANBgkqhkiG9w0BAQsFADBa\n" \
"MQswCQYDVQQGEwJJRTESMBAGA1UEChMJQmFsdGltb3JlMRMwEQYDVQQLEwpDeWJl\n" \
"clRydXN0MSIwIAYDVQQDExlCYWx0aW1vcmUgQ3liZXJUcnVzdCBSb290MB4XDTE2\n" \
"MDUyMDEyNTIzOFoXDTI0MDUyMDEyNTIzOFowgYsxCzAJBgNVBAYTAlVTMRMwEQYD\n" \
"VQQIEwpXYXNoaW5ndG9uMRAwDgYDVQQHEwdSZWRtb25kMR4wHAYDVQQKExVNaWNy\n" \
"b3NvZnQgQ29ycG9yYXRpb24xFTATBgNVBAsTDE1pY3Jvc29mdCBJVDEeMBwGA1UE\n" \
"AxMVTWljcm9zb2Z0IElUIFRMUyBDQSA0MIICIjANBgkqhkiG9w0BAQEFAAOCAg8A\n" \
"MIICCgKCAgEAq+XrXaNrOZ71NIgSux1SJl19CQvGeY6rtw7fGbLd7g/27vRW5Ebi\n" \
"kg/iZwvjHHGk1EFztMuZFo6/d32wrx5s7XEuwwh3Sl6Sruxa0EiB0MXpoPV6jx6N\n" \
"XtOtksDaxpE1MSC5OQTNECo8lx0AnpkYGAnPS5fkyfwA8AxanTboskDBSqyEKKo9\n" \
"Rhgrp4qs9K9LqH5JQsdiIMDmpztd65Afu4rYnJDjOrFswpTOPjJry3GzQS65xeFd\n" \
"2FkngvvhSA1+6ATx+QEnQfqUWn3FMLu2utcRm4j6AcxuS5K5+Hg8y5xomhZmiNCT\n" \
"sCqDLpcRHX6BIGHksLmbnG5TlZUixtm9dRC62XWMPD8d0Jb4M0V7ex9UM+VIl6cF\n" \
"JKLb0dyVriAqfZaJSHuSetAksd5IEfdnPLTf+Fhg9U97NGjm/awmCLbzLEPbT8QW\n" \
"0JsMcYexB2uG3Y+gsftm2tjL6fLwZeWO2BzqL7otZPFe0BtQsgyFSs87yC4qanWM\n" \
"wK5c2enAfH182pzjvUqwYAeCK31dyBCvLmKM3Jr94dm5WUiXQhrDUIELH4Mia+Sb\n" \
"vCkigv2AUVx1Xw41wt1/L3pnnz2OW4y7r530zAz7qB+dIcHz51IaXc4UV21QuEnu\n" \
"sQsn0uJpJxJuxsAmPuekKxuLUzgG+hqHOuBLf5kWTlk9WWnxcadlZRsCAwEAAaOC\n" \
"AUIwggE+MB0GA1UdDgQWBBR6e4zBz+egyhzUa/r74TPDDxqinTAfBgNVHSMEGDAW\n" \
"gBTlnVkwgkdYzKz6CFQ2hns6tQRN8DASBgNVHRMBAf8ECDAGAQH/AgEAMA4GA1Ud\n" \
"DwEB/wQEAwIBhjAnBgNVHSUEIDAeBggrBgEFBQcDAQYIKwYBBQUHAwIGCCsGAQUF\n" \
"BwMJMDQGCCsGAQUFBwEBBCgwJjAkBggrBgEFBQcwAYYYaHR0cDovL29jc3AuZGln\n" \
"aWNlcnQuY29tMDoGA1UdHwQzMDEwL6AtoCuGKWh0dHA6Ly9jcmwzLmRpZ2ljZXJ0\n" \
"LmNvbS9PbW5pcm9vdDIwMjUuY3JsMD0GA1UdIAQ2MDQwMgYEVR0gADAqMCgGCCsG\n" \
"AQUFBwIBFhxodHRwczovL3d3dy5kaWdpY2VydC5jb20vQ1BTMA0GCSqGSIb3DQEB\n" \
"CwUAA4IBAQAR/nIGOiEKN27I9SkiAmKeRQ7t+gaf77+eJDUX/jmIsrsB4Xjf0YuX\n" \
"/bd38YpyT0k66LMp13SH5LnzF2CHiJJVgr3ZfRNIfwaQOolm552W95XNYA/X4cr2\n" \
"du76mzVIoZh90pMqT4EWx6iWu9El86ZvUNoAmyqo9DUA4/0sO+3lFZt/Fg/Hjsk2\n" \
"IJTwHQG5ElBQmYHgKEIsjnj/7cae1eTK6aCqs0hPpF/kixj/EwItkBE2GGYoOiKa\n" \
"3pXxWe6fbSoXdZNQwwUS1d5ktLa829d2Wf6l1uVW4f5GXDuK+OwO++8SkJHOIBKB\n" \
"ujxS43/jQPQMQSBmhxjaMmng9tyPKPK9\n" \
"-----END CERTIFICATE-----\n";

WiFiClientSecure client;


const float WHEEL_CIRCUMFERENCE = 2.2332f;

const int I2C_SDA = 19;
const int I2C_SCL = 18;
Adafruit_BMP085 bmp;

static int rotationCountPressure = MEASURE_PRESSURE_EVERY_NTH_ROTATION;
static int rotationCountDate = RECORD_NTH_ROTATION;
static RTC_DATA_ATTR struct timeval sleepEnterTime;
static RTC_DATA_ATTR int bootCount = 0;
static timeval lastEventTime = { 0UL, 0UL };
static timeval lastActivityTime = { 0UL, 0UL };

static int32_t presure = 0;

QueueHandle_t  queue = NULL;
const int QUEUE_SIZE = 100;


Preferences preferences;

/* Pass evet type to the main program using a queue. */
static void IRAM_ATTR handleReedInterrupt(void *arg)
{
	uint32_t intr_status = PCNT.int_st.val;
	int i;
	pcnt_evt_t evt;
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

	if (intr_status & (BIT(PCNT_UNIT))) {
		state = !state;
		digitalWrite(LED_BUILTIN, state);

		evt.time = kalfy::time::getCurrentTime();
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


typedef struct riderMessage_t
{
	unsigned long count;
	timeval time;
};


void IRAM_ATTR handleButtonInterrupt() {
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
	state = !state;
	digitalWrite(LED_BUILTIN, state);

	riderMessage_t msg;
	msg.count = isrCounter;
	msg.time = kalfy::time::getCurrentTime();;
	xQueueSendToBackFromISR(queue, (void *)&msg, &xHigherPriorityTaskWoken);

	if (xHigherPriorityTaskWoken)
	{
		portYIELD_FROM_ISR();
	}
}

/* time to block the task until data is available */
const TickType_t xTicksToWait = pdMS_TO_TICKS(100);

void saveRotationTask(void *pvParameter)
{
	pcnt_evt_t msg;
	BaseType_t xStatus;

	if (queue == NULL) {
		printf("Queue is not ready");
		return;
	}
	while (1) {
		xStatus = xQueueReceive(pcnt_evt_queue, &msg, (TickType_t)(10 / portTICK_PERIOD_MS));
		if (xStatus == pdPASS) {
			Serial.print("(");
			Serial.print((time_t)msg.time.tv_sec);
			Serial.print(",");
			Serial.print((suseconds_t)msg.time.tv_usec);
			Serial.println(")");
			kalfy::record::saveRevolution(msg.time);	
			kalfy::record::savePressure(presure);
			lastActivityTime = msg.time;
		}

	    vTaskDelay(100 / portTICK_PERIOD_MS); //wait for 100 ms
	}
}


void mainTask(void *pvParameter)
{
	riderMessage_t msg;
	BaseType_t xStatus;
	unsigned long altitudeUpdateStart = 0UL;
	unsigned long activityUpdateStart = 0UL;

	if (queue == NULL) {
		printf("Queue is not ready");
		return;
	}

	dailyUplinkSetup();
	while (1) {
		xStatus = xQueueReceive(queue, &msg, (TickType_t)(0 / portTICK_PERIOD_MS));
		if (xStatus == pdPASS) {
			if (1) {
				Serial.println("--- Printing file.");
				bool deleteFile = false;


				// Disable interrupts as displaying file takes some time
				pcnt_intr_disable(PCNT_UNIT);
				gpio_intr_disable(BUTTON_PIN);

				kalfy::record::printAll();

				//if (gpio_get_level(BUTTON_PIN) == LOW) {
				//	Serial.println("Deleting file");
				//	kalfy::record::clear();
				//}

				//Serial.println("=== sendData called");
				//kalfy::ble::run();
				//Serial.println("sendData done");
				//btStop();

				//kalfy::record::printAll();

				if (connectWiFi() == WL_CONNECTED) {
					configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
					printLocalTime();

					Serial.println("=== sendData called");
					kalfy::record::uploadAll(ODOCYCLE_SERVER, ODOCYCLE_ID, ODOCYCLE_TOKEN, ODOCYCLE_CERT);
					Serial.println("sendData done");

					//WiFi.disconnect(true);
					//WiFi.mode(WIFI_OFF);
				}

				//kalfy::record::clear();

				lastActivityTime = kalfy::time::getCurrentTime();

				gpio_intr_enable(BUTTON_PIN);
				pcnt_intr_enable(PCNT_UNIT);
			}
			//goToSleep();
		}

		if ((millis() - altitudeUpdateStart) > ALTITUDE_UPDATE_PERIOD_MS) {
			altitudeUpdateStart = millis();
			printLocalTime();
			Serial.printf("Battery voltage: %f V\n", readBatteryVoltage());
			get_altitude();

			int16_t pcntCntr = 0;
			if (pcnt_get_counter_value(PCNT_UNIT, &pcntCntr) == ESP_OK) {
				Serial.print("Read pcnt unit 0: ");
				Serial.println(pcntCntr);
			}
		}

		if ((millis() - activityUpdateStart) > LAST_ACTIVITY_UPDATE_PERIOD_MS) {
			activityUpdateStart = millis();
			timeval now = kalfy::time::getCurrentTime();
			unsigned long delta = kalfy::time::durationBeforeNow(&lastActivityTime, &now);
			Serial.printf("\n\nLast activity: %ld sec|%ld us\n", lastActivityTime.tv_sec, lastActivityTime.tv_usec);
			Serial.printf("Time since last activity: %lu us\n", delta);

			if (delta > MAX_IDLE_TIME_US) {
				goToSleep();
			}
		}
				
		vTaskDelay(1000 / portTICK_PERIOD_MS); //wait for 100 ms
	}
}

void goToSleep() {
	timeval now = kalfy::time::getCurrentTime();
	preferences.begin("RidezTracker", false);
	preferences.putBytes("time_when_sleep", &now, sizeof(now));
	preferences.end();

	rtc_gpio_init(REED_PIN);
	gpio_pullup_dis(REED_PIN);
	gpio_pulldown_en(REED_PIN);

	rtc_gpio_init(BUTTON_PIN);
	gpio_pullup_dis(BUTTON_PIN);
	gpio_pulldown_en(BUTTON_PIN);

	esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
	esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_ON);
	esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_ON);
	esp_sleep_enable_ext1_wakeup(BIT(REED_PIN) | BIT(BUTTON_PIN), ESP_EXT1_WAKEUP_ANY_HIGH);
	//esp_sleep_enable_ext1_wakeup(BIT(REED_PIN), ESP_EXT1_WAKEUP_ANY_HIGH);

	//esp_sleep_enable_ext0_wakeup(REED_PIN, 1); // nefungovalo byl TSWA reset
	rtc_gpio_isolate(GPIO_NUM_12); 

	 //SET_PERI_REG_MASK(RTC_CNTL_SDIO_CONF_REG, RTC_CNTL_XPD_SDIO_REG_M);
	 //SET_PERI_REG_MASK(RTC_CNTL_SDIO_CONF_REG, RTC_CNTL_SDIO_FORCE_M);

	esp_deep_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
	Serial.println("Setup ESP32 to sleep for " + String((unsigned long)TIME_TO_SLEEP) + " Seconds");  // TBD wake up once a day

	gettimeofday(&sleepEnterTime, NULL);

	esp_deep_sleep_start();
}

unsigned long altitudeUpdateStart = 0;




void setup()
{
	Serial.begin(115200);
	delay(1000); //Take some time to open up the Serial Monitor

	Wire.begin(I2C_SDA, I2C_SCL);

	struct timeval now;
	gettimeofday(&now, NULL);
	int sleep_time_ms = (now.tv_sec - sleepEnterTime.tv_sec) * 1000 + (now.tv_usec - sleepEnterTime.tv_usec) / 1000;
	Serial.printf("\n\nSleep time %d sec\n", sleep_time_ms / 1000);

	++bootCount;
	Serial.println("Boot number: " + String(bootCount));

	lastActivityTime = now;

	preferences.begin("RidezTracker", false);
	unsigned int resetTimes = preferences.getUInt("resetTimes", 0);
	resetTimes++;
	Serial.printf("Number of restart times: %d\n", resetTimes);
	preferences.putUInt("resetTimes", resetTimes);

	if (now.tv_sec < 10000) {
		
		preferences.getBytes("time_when_sleep", &now, sizeof(now));
		Serial.printf("Loading time when going to sleep sleep: %ul\n", now.tv_sec);
		struct timezone tz;
		tz.tz_minuteswest = 60;
		tz.tz_dsttime = 60;
		settimeofday(&now, &tz);
		time_t t = time(NULL);
		log_d("Time: %s", ctime(&t));
	}

	preferences.end();

	woken();
}

void normalModeSetup() {
	queue = xQueueCreate(QUEUE_SIZE, sizeof(riderMessage_t));

	if (queue == NULL) {
		Serial.println("Error creating the queue");
	}

	pinMode(BUTTON_PIN, INPUT_PULLDOWN);
	attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonInterrupt, RISING);

	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, state);

	pcnt_evt_queue = xQueueCreate(10, sizeof(pcnt_evt_t));
	if (pcnt_evt_queue == NULL) {
		Serial.println("Error creating the queue");
	}

	initPcnt();
	pinMode(REED_PIN, INPUT_PULLDOWN);

	xTaskCreate(
		mainTask,     /* Task function. */
		"Consumer",       /* String with name of task. */
		8128,            /* Stack size in words. */
		NULL,             /* Parameter passed as input of the task */
		9,               /* Priority of the task. */
		NULL);            /* Task handle. */

	xTaskCreate(
		saveRotationTask,     /* Task function. */
		"ReedConsumer",       /* String with name of task. */
		2048,            /* Stack size in words. */
		NULL,             /* Parameter passed as input of the task */
		10,               /* Priority of the task. */
		NULL);            /* Task handle. */
}

void onDemandUplinkSetup() {
	Serial.println("=== sendData called");
	connectWiFi();
	kalfy::record::uploadAll(ODOCYCLE_SERVER, ODOCYCLE_ID, ODOCYCLE_TOKEN, ODOCYCLE_CERT);
	Serial.println("sendData done");
	WiFi.disconnect(true);
	WiFi.mode(WIFI_OFF);
}

void dailyUplinkSetup() {
	connectWiFi();
	configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
	Serial.println("=== sendData called");
	kalfy::record::uploadAll(ODOCYCLE_SERVER, ODOCYCLE_ID, ODOCYCLE_TOKEN, ODOCYCLE_CERT);
	Serial.println("sendData done");
	WiFi.disconnect(true);
	WiFi.mode(WIFI_OFF);
}

void get_altitude() {
	Wire.begin(I2C_SDA, I2C_SCL);
	if (bmp.begin(BMP085_HIGHRES))
	{
		presure = bmp.readPressure();
		Serial.printf("Temperature: %f C, Pressure: %d Pa, Altitude %d m\n", bmp.readTemperature(), presure, bmp.readAltitude());
	}
	else
	{
		Serial.println("Could not find BMP180 sensor");
	}
}


void woken()
{
	esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
	uint32_t triggerSource;

	switch (wakeup_reason)
	{
	case 1:
		// Serial.println("Wakeup caused by external signal using RTC_IO");
		break;
	case 2:
		// Serial.println("Wakeup caused by external signal using RTC_CNTL");
		triggerSource = (uint32_t)esp_sleep_get_ext1_wakeup_status();
		Serial.printf("Source %x", triggerSource);
		if ((triggerSource & TRIGGERED_BY_REED) > 0)
		{
			Serial.printf("Entering Normal mode");
			normalModeSetup();
		}
		else if ((triggerSource & TRIGGERED_BY_BUTTON) > 0)
		{
			Serial.printf("Entering On Demand Uplink mode");
			onDemandUplinkSetup();
			goToSleep();
		}
		break;
	case 3:
		{
			Serial.printf("Entering On Demand Uplink mode");
			dailyUplinkSetup();
			goToSleep();
		}
		break;
	default:
		Serial.println("Wakeup was not caused by deep sleep");
		//dailyUplinkSetup();
		normalModeSetup();
		break;
	}
}

esp_err_t event_handler(void *ctx, system_event_t *event)
{
	return ESP_OK;
}

void scanWiFi() {
	// WiFi.scanNetworks will return the number of networks found
	int n = WiFi.scanNetworks();
	Serial.println("scan done");
	if (n == 0) {
		Serial.println("no networks found");
	}
	else {
		Serial.print(n);
		Serial.println(" networks found");
		for (int i = 0; i < n; ++i) {
			// Print SSID and RSSI for each network found
			Serial.print(i + 1);
			Serial.print(": ");
			Serial.print(WiFi.SSID(i));
			Serial.print(" (");
			Serial.print(WiFi.RSSI(i));
			Serial.print(")");
			Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
			delay(10);
		}
	}
	Serial.println("");
}

wl_status_t connectWiFi() {
	// Connect to WiFi access point.
	Serial.println();
	Serial.println();
	
	Serial.print("Connecting to ");
	Serial.println(WLAN_SSID);

    //WiFi.mode(WIFI_OFF);
    delay(2000);
	Serial.println("Connecting Wifi ");
	for (int loops = MAX_WIFI_CONNECTION_ATTEMPTS; loops > 0; loops--) {
  
		WiFi.disconnect(true);                                      // Clear Wifi Credentials
        WiFi.persistent(false);                                     // Avoid to store Wifi configuration in Flash
        WiFi.mode(WIFI_STA);                                        // Ensure WiFi mode is Station 
    
		WiFi.begin(WLAN_SSID, WLAN_PASS);
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
	byte mac[6];
	WiFi.macAddress(mac);

	Serial.print("MAC: ");
	Serial.print(mac[0], HEX);
	Serial.print(":");
	Serial.print(mac[1], HEX);
	Serial.print(":");
	Serial.print(mac[2], HEX);
	Serial.print(":");
	Serial.print(mac[3], HEX);
	Serial.print(":");
	Serial.print(mac[4], HEX);
	Serial.print(":");
	Serial.println(mac[5], HEX);


	//nvs_flash_init();
	//tcpip_adapter_init();
	//ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
	//wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	//ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	//ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	//ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	//wifi_config_t sta_config;
	//sta_config.sta.ssid = "sde-guest"; // WLAN_SSID;
	//sta_config.sta.password = "4Our6uest"; // WLAN_PASS;
	//sta_config.sta.bssid_set = false;

	//ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
	//ESP_ERROR_CHECK(esp_wifi_start());
	//ESP_ERROR_CHECK(esp_wifi_connect());


	return WiFi.status();
}


float readBatteryVoltage() { 
	const float DEFAULT_VREF = 1.1f;
 	const float ADC_RESOLUTION = 4096.0f;  // 12bit ADC = 2^12
	const float REFERENCE_VOLTAGE = 4.8f / 0.23f; // 1.0f; // nominal EPS voltage
	const float VOLTAGE_DIVIDER = 1.0f; // 122.0f / 22.0f;  // LiPo battery 3.7V to 0.66V, 22kOhm+100kOhm
	return VOLTAGE_DIVIDER * REFERENCE_VOLTAGE * float(analogRead(BATTERY_PIN)) / ADC_RESOLUTION;  // LiPo battery
	//Serial.print("Battery Voltage = "); 
	//Serial.print(VBAT, 2); 
	//Serial.println(" V");

}

void printLocalTime()
{
	struct tm timeinfo;
	if (!getLocalTime(&timeinfo)) {
		Serial.println("Failed to obtain time");
		return;
	}
	Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}


void loop()
{
	vTaskDelete(NULL);
}

static void initPcnt(void)
{
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
	pcnt_set_filter_value(PCNT_UNIT, 1000);
	pcnt_filter_enable(PCNT_UNIT);

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
}
