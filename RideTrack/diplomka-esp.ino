#include <driver/rtc_io.h>
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <WiFi.h>
#include "record.hpp"

#define uS_TO_MS 1000
#define HAS_CONTACT 1
#define NO_CONTACT 0

const gpio_num_t REED_PIN = GPIO_NUM_13;
const gpio_num_t BUTTON_PIN = GPIO_NUM_12;
const uint32_t TRIGGERED_BY_REED = BIT(13);
const uint32_t TRIGGERED_BY_BUTTON = BIT(12);
const uint64_t SLEEP_INTERVAL_uS = 20 * uS_TO_MS;

const char* ssid = "Zakazane uzemi";
const char* password = "X2@C0SpbXJq9tw^";
const char* deviceId = "id-zarizeni";
const char* apiUrl = "http://odocycle20171225044036.azurewebsites.net/api";

#define I2C_SDA 18
#define I2C_SCL 19
Adafruit_BMP085 bmp;
#define MEASURE_PRESSURE_EVERY_NTH_ROTATION 100
RTC_DATA_ATTR int rotationCountPressure = MEASURE_PRESSURE_EVERY_NTH_ROTATION;
#define RECORD_NTH_ROTATION 10
RTC_DATA_ATTR int rotationCountDate = RECORD_NTH_ROTATION;


void woken()
{
	esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
	uint32_t triggerSource;

	switch (wakeup_reason)
	{
	case 1:
		Serial.println("Wakeup caused by external signal using RTC_IO");
		break;
	case 2:
		Serial.println("Wakeup caused by external signal using RTC_CNTL");
		triggerSource = (uint32_t)esp_sleep_get_ext1_wakeup_status();
		if (triggerSource == TRIGGERED_BY_REED)
		{
			rotationDetected();
		}
		else if (triggerSource == TRIGGERED_BY_BUTTON)
		{
			sendData();
		}
		waitForLosingContact();
		break;
	case 3:
		Serial.println("Wakeup caused by timer");
		waitForLosingContact();
		break;
	case 4:
		Serial.println("Wakeup caused by touchpad");
		break;
	case 5:
		Serial.println("Wakeup caused by ULP program");
		break;
	default:
		Serial.println("Wakeup was not caused by deep sleep");
		break;
	}
}

void rotationDetected()
{
	// save date
	if (rotationCountDate == RECORD_NTH_ROTATION)
	{
		kalfy::record::saveDate();
		rotationCountDate = 1;
	}
	else
	{
		rotationCountDate++;
	}

	// pressure measurement
	if (MEASURE_PRESSURE_EVERY_NTH_ROTATION == rotationCountPressure)
	{
		Wire.begin(I2C_SDA, I2C_SCL);
		if (bmp.begin(BMP085_HIGHRES))
		{
			kalfy::record::savePressure(bmp.readPressure());
			// Serial.printf("Temperature: %f C, Pressure: %d Pa\n", bmp.readTemperature(), bmp.readPressure());
		}
		else
		{
			Serial.println("Could not find BMP180 sensor");
		}
		rotationCountPressure = 0;
	}
	else
	{
		rotationCountPressure++;
	}
}

void sendData()
{
	Serial.println("=== sendData called");
	connectToWiFi();
	kalfy::record::uploadAll(apiUrl, deviceId);
}

void connectToWiFi()
{
	Serial.println("== Connecting to WiFi");
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED)
	{
#ifdef _DEBUG
		if (WiFi.status() == WL_IDLE_STATUS)
		{
			Serial.println(
				"it is a temporary status assigned when WiFi.begin() is called and remains active until the number of attempts expires (resulting in WL_CONNECT_FAILED) or a connection is established (resulting in WL_CONNECTED)");
		}
		else if (WiFi.status() == WL_CONNECT_FAILED)
		{
			Serial.println("assigned when the connection fails for all the attempts");
		}
		else if (WiFi.status() == WL_DISCONNECTED)
		{
			Serial.println("assigned when disconnected from a network");
		}
		Serial.print(".");
#endif // _DEBUG
		delay(500);
	}
	Serial.println("== WiFi connected");
}

void waitForLosingContact()
{
	// "After wake up from sleep, IO pad(s) used for wakeup will be configured as RTC IO. Before using these pads as digital GPIOs, reconfigure them using rtc_gpio_deinit(gpio_num) function."
	// zdroj: https://github.com/espressif/esp-idf/blob/master/docs/api-reference/system/sleep_modes.rst
	rtc_gpio_deinit(REED_PIN);
	rtc_gpio_deinit(BUTTON_PIN);
	/*Serial.println("=== PIN STATUS ===");
	Serial.println(digitalRead(REED_PIN));
	Serial.println(digitalRead(BUTTON_PIN));
	Serial.println("=== PIN STATUS END ===");*/
	if (digitalRead(REED_PIN) == HAS_CONTACT || digitalRead(BUTTON_PIN) == HAS_CONTACT)
	{
		Serial.println("=== DEBUG: still has contact ===");
		esp_deep_sleep(SLEEP_INTERVAL_uS);
	}
	Serial.println("=== DEBUG: contact lost ===");
}

void setup()
{
	Serial.begin(115200);

	woken();

	rtc_gpio_init(REED_PIN);
	gpio_pullup_dis(REED_PIN);
	gpio_pulldown_en(REED_PIN);

	rtc_gpio_init(BUTTON_PIN);
	gpio_pullup_dis(BUTTON_PIN);
	gpio_pulldown_en(BUTTON_PIN);

	esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
	// dokumentace k funkci:
	// https://github.com/espressif/esp-idf/blob/master/components/esp32/include/esp_sleep.h
	// https://github.com/espressif/esp-idf/blob/master/components/esp32/test/test_sleep.c
	esp_sleep_enable_ext1_wakeup(BIT(REED_PIN) | BIT(BUTTON_PIN), ESP_EXT1_WAKEUP_ANY_HIGH);

	esp_deep_sleep_start();
}

void loop()
{
}
