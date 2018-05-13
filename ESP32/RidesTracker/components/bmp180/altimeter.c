#include "altimeter.h"
#include "bmp180.h"
#include <driver/i2c.h>
#include <esp_err.h>
#include <esp_log.h>

bmp180_dev_t dev;
esp_err_t res;

static const char *TAG = "Altimeter";

void initAltimeter(void) {
    while (i2cdev_init() != ESP_OK)
    {
        printf("Could not init I2Cdev library\n");
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }

    while (bmp180_init_desc(&dev, I2C_NUM_1, SDA_GPIO, SCL_GPIO) != ESP_OK)
    {
        printf("Could not init device descriptor\n");
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }

    while ((res = bmp180_init(&dev)) != ESP_OK)
    {
        printf("Could not init BMP180, err: %d\n", res);
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
}

void getAltitude(uint32_t *pressure) {
    float temp;
    esp_log_write(ESP_LOG_INFO, TAG, "Current core: %d\n", xPortGetCoreID());

    res = bmp180_measure(&dev, &temp, pressure, BMP180_MODE_STANDARD);
    if (res != ESP_OK)
        esp_log_write(ESP_LOG_ERROR, TAG, "Could not measure: %d\n", res);
    else
        esp_log_write(ESP_LOG_INFO, TAG, "Temperature: %.2f degrees Celsius; Pressure: %d MPa\n", temp, *pressure);
}
