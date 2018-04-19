#include <driver/rtc_io.h>
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <HTTPClient.h>
#include "record.hpp"
#include "time.hpp"

#define uS_TO_MS 1000
#define HAS_CONTACT 1
#define NO_CONTACT 0

const gpio_num_t LED_BUILTIN = GPIO_NUM_2;
const gpio_num_t REED_PIN = GPIO_NUM_13;
const gpio_num_t BUTTON_PIN = GPIO_NUM_12;
const uint32_t TRIGGERED_BY_REED = BIT(13);
const uint32_t TRIGGERED_BY_BUTTON = BIT(12);
const uint64_t SLEEP_INTERVAL_uS = 20 * uS_TO_MS;
const uint64_t ALTITUDE_UPDATE_PERIOD = 3000;

static uint64_t frameCount = 0;
int buffer_tail = 0;
int buffer_head = 0;
const byte interruptPin = GPIO_NUM_12;
volatile int interruptCounter = 0;
static uint64_t numberOfInterrupts = 0;

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

const char* ssid = "NAHATCH";
const char* password = "nahatch123";

#define WLAN_SSID       "sde-guest"
#define WLAN_PASS       "4Our6uest"

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

#define I2C_SDA 18
#define I2C_SCL 19
Adafruit_BMP085 bmp;

#define MEASURE_PRESSURE_EVERY_NTH_ROTATION 100
#define RECORD_NTH_ROTATION 10
static RTC_DATA_ATTR int rotationCountPressure = MEASURE_PRESSURE_EVERY_NTH_ROTATION;
static RTC_DATA_ATTR int rotationCountDate = RECORD_NTH_ROTATION;

static RTC_DATA_ATTR struct timeval sleepEnterTime;
static RTC_DATA_ATTR int bootCount = 0;

static int32_t presure = 0;


void IRAM_ATTR handleInterrupt() {
	portENTER_CRITICAL_ISR(&mux);
	++interruptCounter;
	portEXIT_CRITICAL_ISR(&mux);
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
		else if ( (triggerSource & TRIGGERED_BY_BUTTON) > 0)
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


void setup()
{
	Serial.begin(115200);

	pinMode(interruptPin, INPUT_PULLUP);
	attachInterrupt(digitalPinToInterrupt(interruptPin), handleInterrupt, RISING);

	//connectWiFi();

	
	/* struct timeval now;
	gettimeofday(&now, NULL);
	int sleep_time_ms = (now.tv_sec - sleepEnterTime.tv_sec) * 1000 + (now.tv_usec - sleepEnterTime.tv_usec) / 1000;

	//Serial.begin(115200);
	//Serial.printf("sleep_time_ms %d", sleep_time_ms);
	*/
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, HIGH);

	++bootCount;
	Serial.println("Boot number: " + String(bootCount));

	/*
	
	//const int wakeup_time_sec = 5;
	//printf("Enabling timer wakeup, %ds\n", wakeup_time_sec);
	//esp_sleep_enable_timer_wakeup(wakeup_time_sec * 1000000);
	woken();

	rtc_gpio_init(REED_PIN);
	gpio_pullup_dis(REED_PIN);
	gpio_pulldown_en(REED_PIN);

	rtc_gpio_init(BUTTON_PIN);
	gpio_pullup_dis(BUTTON_PIN);
	gpio_pulldown_en(BUTTON_PIN);

	esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
	// dokumentace k funkci:
	// http://esp-idf.readthedocs.io/en/latest/api-reference/system/sleep_modes.html
	// https://github.com/espressif/esp-idf/blob/master/components/esp32/include/esp_sleep.h
	// https://github.com/espressif/esp-idf/blob/master/components/esp32/test/test_sleep.c
	esp_sleep_enable_ext1_wakeup(BIT(REED_PIN) | BIT(BUTTON_PIN), ESP_EXT1_WAKEUP_ANY_HIGH);

	
	esp_deep_sleep_start();

	*/
}

void get_altitude() {
	Wire.begin(SDA, SCL);
	//Wire.begin(I2C_SDA, I2C_SCL);
	if (bmp.begin(BMP085_HIGHRES))
	{
		presure = bmp.readPressure();
		kalfy::record::savePressure(presure);
		Serial.printf("Temperature: %f C, Pressure: %d Pa, Altitude %d m\n", bmp.readTemperature(), presure, bmp.readAltitude());

	}
	else
	{
		Serial.println("Could not find BMP180 sensor");
	}
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
	

	if (interruptCounter > 0) {

		portENTER_CRITICAL(&mux);
		interruptCounter--;
		portEXIT_CRITICAL(&mux);
        
		numberOfInterrupts++;

		Serial.print("Relovolution detected. Total: ");
		Serial.println((long unsigned int)numberOfInterrupts);
		kalfy::record::saveRevolution(numberOfInterrupts, WHEEL_CIRCUMFERENCE);
		//adafruitSendData(String("revolution"), numberOfInterrupts);
	}
    

	if ((millis() - altitudeUpdateStart) > ALTITUDE_UPDATE_PERIOD) {
		altitudeUpdateStart = millis();
		get_altitude();


		//adafruitSendData(String("presure"), presure);
	}
	
    //delay(200);
}
