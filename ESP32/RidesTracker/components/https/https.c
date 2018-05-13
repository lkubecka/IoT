#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "esp_tls.h"

#include "https.h"

static const char *TAG = "HTTPS";

#define  ODOCYCLE_SERVER "ridestracker.azurewebsites.net"
#define  ODOCYCLE_URL "https://ridestracker.azurewebsites.net/api/v1/Records/"
#define  ODOCYCLE_ID "4f931a53-6e1b-4e85-bbda-7c71d9f8f2b9"
#define  ODOCYCLE_TOKEN "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiI5YmQwYzhmMC05ZTI0LTQxZTgtYjkwNi02ZDI3MGFjNGFkN2UiLCJqdGkiOiIxNzAxZmU2MC1kZjYxLTQ4ZDktYjIyMy0zODc3MDhjZjIxYWEiLCJleHAiOjE1Mjg4MDgyMzUsImlzcyI6ImF6dXJlLWRldiIsImF1ZCI6ImF6dXJlLWRldiJ9.l3mXH7Ywt8OY5E0gvrRLXMBK5LqrQbXL8dMHMR5mekI"

/************************* Adafruit.io Setup *********************************/
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  8883                   // 8883 for MQTTS
#define AIO_USERNAME    "lkubecka"
#define AIO_KEY         "083e7c316c8b46d2bf0d27f858ee1c54"

#define AIO_HOST "io.adafruit.com"
#define AIO_KEY "083e7c316c8b46d2bf0d27f858ee1c54"
#define AIO_URL "io.adafruit.com/api/groups/default/send.json?x-aio-key=083e7c316c8b46d2bf0d27f858ee1c54&"

static const char *RIDES_REQUEST = "POST " ODOCYCLE_URL " HTTP/1.0\r\n" \
    "Host: " ODOCYCLE_SERVER "\r\n" \
    "User-Agent: odocycle-esp32\r\n" \
    "Authorization: Bearer " ODOCYCLE_TOKEN "\r\n" \
    "Content-Type: application/form-data\r\n" \
    "\r\n";

static const char *AIO_REQUEST = "GET " AIO_URL "pressure=123 HTTP/1.0\r\n" \
    "Host: " AIO_SERVER "\r\n" \
    "User-Agent: odocycle-esp32\r\n" \
    "\r\n";

void https_handle_request(esp_tls_t *tls, const char * request) {
    if(tls == NULL) {
        return;
    }

    size_t written_bytes = 0;
    do {
        int ret = esp_tls_conn_write(tls, 
                                    request + written_bytes, 
                                    strlen(request) - written_bytes);
        if (ret >= 0) {
            ESP_LOGI(TAG, "%d bytes written", ret);
            written_bytes += ret;
        } else if (ret != MBEDTLS_ERR_SSL_WANT_READ  && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "esp_tls_conn_write  returned 0x%x", ret);
            esp_tls_conn_delete(tls);
        }
    } while(written_bytes < strlen(request));
}

void https_handle_response(esp_tls_t * tls) {
    if(tls == NULL) {
        return;
    }

    char buf[512];

    ESP_LOGI(TAG, "Reading HTTP response...");
    do
    {
        int len = sizeof(buf) - 1;
        bzero(buf, sizeof(buf));
        int ret = esp_tls_conn_read(tls, (char *)buf, len);
        
        if(ret == MBEDTLS_ERR_SSL_WANT_WRITE  || ret == MBEDTLS_ERR_SSL_WANT_READ)
            continue;
        
        if(ret < 0)
        {
            ESP_LOGE(TAG, "esp_tls_conn_read  returned -0x%x", -ret);
            break;
        }

        if(ret == 0)
        {
            ESP_LOGI(TAG, "connection closed");
            break;
        }

        len = ret;
        ESP_LOGD(TAG, "%d bytes read", len);
        /* Print response directly to stdout as it is read */
        for(int i = 0; i < len; i++) {
            putchar(buf[i]);
        }
    } while(1);
}

void https_request_task(void *pvParameters)
{
    esp_tls_cfg_t cfg = {
        .cacert_pem_buf  = server_root_cert_pem_start,
        .cacert_pem_bytes = server_root_cert_pem_end - server_root_cert_pem_start,
    };
    
    struct esp_tls *tls = esp_tls_conn_http_new(WEB_URL, &cfg);
    
    if(tls != NULL) {
        ESP_LOGI(TAG, "Connection established...");
        ESP_LOGI(TAG, "\nRequest:%s\n", AIO_REQUEST);

        https_handle_request(tls, AIO_REQUEST);
        https_handle_response(tls);
        esp_tls_conn_delete(tls);  
        //putchar('\n'); // JSON output doesn't have a newline at end
    } else {
        ESP_LOGE(TAG, "Connection failed...");
    } 
    
    vTaskDelete(NULL);
}
