#include <driver/rtc_io.h>
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <HTTPClient.h>
#include "record.hpp"
#include "time.hpp"

#include "driver/pcnt.h"

#define _DEBUG

#define uS_TO_MS 1000
#define HAS_CONTACT 1
#define NO_CONTACT 0
volatile byte state = HIGH;


const int MEASURE_PRESSURE_EVERY_NTH_ROTATION = 100;
const int RECORD_NTH_ROTATION = 10;
const long MIN_SAMPLING_TIME_US = 85;

const gpio_num_t LED_BUILTIN = GPIO_NUM_2;
const gpio_num_t REED_PIN = GPIO_NUM_13;
const gpio_num_t BUTTON_PIN = GPIO_NUM_12;
const uint32_t TRIGGERED_BY_REED = BIT(13);
const uint32_t TRIGGERED_BY_BUTTON = BIT(12);
const uint64_t SLEEP_INTERVAL_uS = 20 * uS_TO_MS;
const uint64_t ALTITUDE_UPDATE_PERIOD_MS = 2000;


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

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

//#define WLAN_SSID       "sde-guest"
//#define WLAN_PASS       "4Our6uest"

#define WLAN_SSID       "NAHATCH"
#define WLAN_PASS       "nahatch123"

/************************* Adafruit.io Setup *********************************/
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  8883                   // 8883 for MQTTS
#define AIO_USERNAME    "lkubecka"
#define AIO_KEY         "083e7c316c8b46d2bf0d27f858ee1c54"

const char* host = "io.adafruit.com";
const char* io_key = "083e7c316c8b46d2bf0d27f858ee1c54";
const char* adafruitPath = "/api/groups/default/send.json?x-aio-key=083e7c316c8b46d2bf0d27f858ee1c54&";

WiFiClientSecure client;


const char* deviceId = "id-zarizeni";
const char* apiUrl = "http://odocycle20171225044036.azurewebsites.net/api";

const float WHEEL_CIRCUMFERENCE = 2.2332f;

const int I2C_SDA = 19;
const int I2C_SCL = 18;
Adafruit_BMP085 bmp;



static RTC_DATA_ATTR int rotationCountPressure = MEASURE_PRESSURE_EVERY_NTH_ROTATION;
static RTC_DATA_ATTR int rotationCountDate = RECORD_NTH_ROTATION;
static RTC_DATA_ATTR struct timeval sleepEnterTime;
static RTC_DATA_ATTR int bootCount = 0;
static RTC_DATA_ATTR timeval lastEventTime = { 0UL, 0UL };

static int32_t presure = 0;

QueueHandle_t  queue = NULL;
const int QUEUE_SIZE = 100;


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
	timeval now = kalfy::time::getCurrentTime();
	long delta = kalfy::time::durationBeforeNow(&lastEventTime, &now);
	
	if (delta > MIN_SAMPLING_TIME_US) {                 // Debounce pulses shorter than MIN_SAMPLING_TIME_US
		if (1) { //++isrCounter % RECORD_NTH_ROTATION) {       // Record every RECORD_NTH_ROTATION
			state = !state;
			digitalWrite(LED_BUILTIN, state);
			riderMessage_t msg;
			msg.count = isrCounter;
			msg.time = now;
			xQueueSendToBackFromISR(queue, (void *)&msg, &xHigherPriorityTaskWoken);
		}
	}
	lastEventTime = now;
	//Serial.println(delta);
	

	//Now the buffer is empty we can switch context if necessary.
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
		xStatus = xQueueReceive(pcnt_evt_queue, &msg, (TickType_t)(1000 / portTICK_PERIOD_MS));
		if (xStatus == pdPASS) {
			Serial.print("(");
			Serial.print((time_t)msg.time.tv_sec);
			Serial.print(",");
			Serial.print((suseconds_t)msg.time.tv_usec);
			Serial.println(")");
			kalfy::record::saveRevolution(msg.time);	
		}

		//vTaskDelay(500 / portTICK_PERIOD_MS); //wait for 500 ms
	}
}


void consumerTask(void *pvParameter)
{
	riderMessage_t msg;
	BaseType_t xStatus;

	if (queue == NULL) {
		printf("Queue is not ready");
		return;
	}
	while (1) {
		xStatus = xQueueReceive(queue, &msg, (TickType_t)(1000 / portTICK_PERIOD_MS));
		if (xStatus == pdPASS) {
			Serial.println("--- Printing file.");
			bool deleteFile = false;


			// Disable interrupts as displaying file takes some time
			pcnt_intr_disable(PCNT_UNIT); 
			gpio_intr_disable(BUTTON_PIN);
			kalfy::record::printAll();
		
			if (gpio_get_level(BUTTON_PIN) == LOW) {
				Serial.println("Deleting file");
				kalfy::record::clear();
			}

			gpio_intr_enable(BUTTON_PIN);
			pcnt_intr_enable(PCNT_UNIT);


		}
		
		//vTaskDelay(500 / portTICK_PERIOD_MS); //wait for 500 ms
	}
}

unsigned long altitudeUpdateStart = 0;

const char* ca_cert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n" \
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n" \
"MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n" \
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n" \
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n" \
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n" \
"2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n" \
"1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n" \
"q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n" \
"tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n" \
"vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n" \
"BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n" \
"5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n" \
"1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4\n" \
"NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG\n" \
"Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\n" \
"8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\n" \
"pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n" \
"MrY=\n" \
"-----END CERTIFICATE-----\n";


void setup()
{
	Serial.begin(115200);

	queue = xQueueCreate(QUEUE_SIZE, sizeof(riderMessage_t));

	if (queue == NULL) {
		Serial.println("Error creating the queue");
	}

	pinMode(BUTTON_PIN, INPUT_PULLDOWN);
	attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonInterrupt, RISING);

	//pinMode(REED_PIN, INPUT_PULLDOWN);
	//attachInterrupt(digitalPinToInterrupt(REED_PIN), handleReedInterrupt, RISING);

	//connectWiFi();

	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, state);
	
	pcnt_evt_queue = xQueueCreate(10, sizeof(pcnt_evt_t));
	if (pcnt_evt_queue == NULL) {
		Serial.println("Error creating the queue");
	}

	initPcnt();
	pinMode(REED_PIN, INPUT_PULLDOWN);

	xTaskCreate(
		consumerTask,     /* Task function. */
		"Consumer",       /* String with name of task. */
		2048,            /* Stack size in words. */
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
	

	++bootCount;
	Serial.println("Boot number: " + String(bootCount));
}



void get_altitude() {
	Wire.begin(I2C_SDA, I2C_SCL);
	if (bmp.begin(BMP085_STANDARD))
	{
		presure = bmp.readPressure();
		//kalfy::record::savePressure(presure);
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
		// Serial.printf("Source %x", triggerSource);
		if ((triggerSource & TRIGGERED_BY_REED) > 0)
		{
			//rotationDetected();
			digitalWrite(LED_BUILTIN, LOW);
			delay(1000);
		}
		else if ((triggerSource & TRIGGERED_BY_BUTTON) > 0)
		{
			//sendData();
			digitalWrite(LED_BUILTIN, LOW);
			delay(1000);
		}
		//waitForLosingContact();
		break;
	case 3:
		// Serial.println("Wakeup caused by timer");
		digitalWrite(LED_BUILTIN, LOW);
		delay(1000);
		//waitForLosingContact();
		break;
	default:
		//Serial.println("Wakeup was not caused by deep sleep");
		break;
	}
}


void sendData()
{
	Serial.println("=== sendData called");
	//connectToWiFi();
	//kalfy::record::uploadAll(apiUrl, deviceId);
}

void connectWiFi() {
	// Connect to WiFi access point.
	Serial.println();
	Serial.println();
	Serial.print("Connecting to ");
	Serial.println(WLAN_SSID);

	WiFi.begin(WLAN_SSID, WLAN_PASS);
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	Serial.println();

	Serial.println("WiFi connected");
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());
}


void adafruitSendData(const String & feed, const uint32_t value) {

	HTTPClient http;

	http.begin("https://" + String(host) + String(adafruitPath) + feed + "=" + String(value), ca_cert);
	log_v("https://" + String(host) + String(adafruitPath) + feed + "=" + String(value));

	// IO API authentication
	http.addHeader("X-AIO-Key", io_key);

	// start connection and send HTTP header
	int httpCode = http.GET();
	Serial.println(httpCode);
	// httpCode will be negative on error
	if (httpCode > 0) {
		// HTTP header has been send and Server response header has been handled
		Serial.printf("[HTTP] GET response: %d\n", httpCode);

		// HTTP 200 OK
		if (httpCode == HTTP_CODE_OK) {
			String payload = http.getString();
			log_v(payload);
		}

		http.end();
	}
}

void loop()
{

	if ((millis() - altitudeUpdateStart) > ALTITUDE_UPDATE_PERIOD_MS) {
		altitudeUpdateStart = millis();
		get_altitude();

		//adafruitSendData(String("presure"), presure);

		int16_t pcntCntr = 0;
		if (pcnt_get_counter_value(PCNT_UNIT, &pcntCntr) == ESP_OK) {
			Serial.print("Read pcnt unit 0: ");
			Serial.println(pcntCntr);
		}
	}


	
    delay(500);
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