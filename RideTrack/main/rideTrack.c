/*
 rideTrack.c - altitude and revolution recording with ESP32 and BMP180 sensor
 
 This file is part of the ESP32 Everest Run project
 https://github.com/lkubecka/IoT/ESP32/rideTrack

 Copyright (c) 2018 Libor Kubecka <libor@kubecka.info>
 This work is licensed under the Apache License, Version 2.0, January 2004
 See the file LICENSE for details.
*/

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_log.h"


#include "driver/gpio.h"
#include "altimeter.h"
#include "bmp180.h"
#include "wifi.h"
#include "weather.h"
#include "revolution_counter.h"


static const char* TAG = "Altimeter";

/* Define pins to connect I2C pressure sensor
*/
#define I2C_PIN_SDA 19 
#define I2C_PIN_SCL 18

RTC_DATA_ATTR static unsigned long reference_pressure = 0l;


// Deep sleep period in seconds
#define DEEP_SLEEP_PERIOD 60
RTC_DATA_ATTR static unsigned long boot_count = 0l;


void measure_altitude()
{
    altitude_data altitude_record = {0};

    esp_log_write(ESP_LOG_INFO, TAG, "Now measuring altitude");
    altitude_record.pressure = (unsigned long) bmp180_read_pressure();
    esp_log_write(ESP_LOG_INFO, TAG, "Presure %lu Pa", altitude_record.pressure);
    altitude_record.temperature = bmp180_read_temperature();
    /* Compensate altitude measurement
       using current reference pressure, preferably at the sea level,
       obtained from weather station on internet
       Assume normal air pressure at sea level of 101325 Pa
       in case weather station is not available.
     */
    altitude_record.reference_pressure = reference_pressure;
    altitude_record.altitude = bmp180_read_altitude(reference_pressure);
    esp_log_write(ESP_LOG_INFO, TAG, "Altitude %0.1f m", altitude_record.altitude);

    time_t now = 0;
    if (time(&now) == -1) {
        esp_log_write(ESP_LOG_WARN, TAG, "Current calendar time is not available");
    } else {
        altitude_record.timestamp = now;
    }

}

void saveRotationTask(void *pvParameter)
{
	pcnt_evt_t msg;
	BaseType_t xStatus;

	if (pcnt_evt_queue == NULL) {
		esp_log_write(ESP_LOG_ERROR, TAG, "Revolution event queue not ready.");
		return;
	}
	while (1) {
		xStatus = xQueueReceive(pcnt_evt_queue, &msg, (TickType_t)(10 / portTICK_PERIOD_MS));
		if (xStatus == pdPASS) {
            esp_log_write(ESP_LOG_INFO, TAG, "t%lu|%lu", (time_t)msg.time.tv_sec, (suseconds_t)msg.time.tv_usec);
			//kalfy::record::saveRevolution(msg.time);	
			//kalfy::record::savePressure(presure);
		}

	    vTaskDelay(100 / portTICK_PERIOD_MS); //wait for 100 ms
	}
}

void app_main()
{
    esp_log_write(ESP_LOG_INFO, TAG, "Starting");

    boot_count++;
    esp_log_write(ESP_LOG_INFO, TAG, "Boot count: %lu", boot_count);

    nvs_flash_init();
    //initialise_wifi();

    initPcnt();
    xTaskCreate(
        saveRotationTask,       /* Task function. */
        "ReedConsumer",         /* String with name of task. */
        2048,                   /* Stack size in words. */
        NULL,                   /* Parameter passed as input of the task */
        10,                     /* Priority of the task. */
        NULL);                  /* Task handle. */


    int count_down = 500;
    // first time update of reference pressure
    if (reference_pressure == 0l) {
        esp_log_write(ESP_LOG_INFO, TAG, "Waiting for reference pressure update...");
        while (1) {
            esp_log_write(ESP_LOG_INFO, TAG, "Number of revolutions %d\n", getNumberOfRevolutions());
            esp_log_write(ESP_LOG_INFO, TAG, "Waiting %d\n", count_down);
            vTaskDelay(1000 / portTICK_RATE_MS);
            if (reference_pressure != 0) {
                esp_log_write(ESP_LOG_INFO, TAG, "Update received");
                break;
            }
            if (--count_down == 0) {
                reference_pressure = 101325l;
                ESP_LOGW(TAG, "Exit waiting. Assumed standard pressure at the sea level\n");
                break;
            }
        }
    }
    
    // esp_log_write(ESP_LOG_INFO, TAG, "Updating altitude.\n");
    // esp_err_t err = bmp180_init(I2C_PIN_SDA, I2C_PIN_SCL);
    // if(err == ESP_OK){
    //     measure_altitude();
    // } else {
    //     esp_log_write(ESP_LOG_ERROR, TAG, "BMP180 init failed with error = %d", err);
    //     vTaskDelay(3000);
    // }

    //esp_log_write(ESP_LOG_INFO, TAG, "Entering deep sleep for %d seconds", DEEP_SLEEP_PERIOD);
    //esp_deep_sleep(1000000LL * DEEP_SLEEP_PERIOD);
}
