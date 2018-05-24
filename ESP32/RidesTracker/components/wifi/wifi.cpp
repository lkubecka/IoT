/*
 wifi.c - Wi-Fi setup routines

 This file is part of the ESP32 Everest Run project
 https://github.com/krzychb/esp32-everest-run

 Copyright (c) 2016 Krzysztof Budzynski <krzychb@gazeta.pl>
 This work is licensed under the Apache License, Version 2.0, January 2004
 See the file LICENSE for details.
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"

#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "wifi.hpp"
#include "Configuration.hpp"
#include <vector>
#include "string.h"

#if 1

static const char* TAG = "Wi-Fi";

std::vector<Configuration> connections = {  
    Configuration("NAHATCH", "nahatch123"),
    Configuration("sde-guest", "4Our6uest"),
    Configuration("NAHATCH", "nahatch123")
};

/* FreeRTOS event group to signal when we are connected & ready to make a request */
EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;


static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

void setup_wifi(void) {
    nvs_flash_init();
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
}

void connect_wifi(Configuration &connection)
{
    ESP_LOGI(TAG, "Initialising");

    wifi_init_config_t init_config = {};
    init_config.event_handler = &esp_event_send;
    init_config.wpa_crypto_funcs = g_wifi_default_wpa_crypto_funcs;
    init_config.static_rx_buf_num = CONFIG_ESP32_WIFI_STATIC_RX_BUFFER_NUM;
    init_config.dynamic_rx_buf_num = CONFIG_ESP32_WIFI_DYNAMIC_RX_BUFFER_NUM;
    init_config.tx_buf_type = CONFIG_ESP32_WIFI_TX_BUFFER_TYPE;
    init_config.static_tx_buf_num = WIFI_STATIC_TX_BUFFER_NUM;
    init_config.dynamic_tx_buf_num = WIFI_DYNAMIC_TX_BUFFER_NUM;
    init_config.ampdu_rx_enable = WIFI_AMPDU_RX_ENABLED;
    init_config.ampdu_tx_enable = WIFI_AMPDU_TX_ENABLED;
    init_config.nvs_enable = WIFI_NVS_ENABLED;
    init_config.nano_enable = WIFI_NANO_FORMAT_ENABLED;
    init_config.tx_ba_win = WIFI_DEFAULT_TX_BA_WIN;
    init_config.rx_ba_win = WIFI_DEFAULT_RX_BA_WIN;
    init_config.magic = WIFI_INIT_CONFIG_MAGIC;
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));

    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid, connection.getSSID(), sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, connection.getPassword(), sizeof(wifi_config.sta.password));

    ESP_LOGI(TAG, "Setting SSID: %s and password: %s", wifi_config.sta.ssid, wifi_config.sta.password);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, 1000 / portTICK_PERIOD_MS);
}


void printNetworkInfo(void) {
    tcpip_adapter_ip_info_t ip;
    memset(&ip, 0, sizeof(tcpip_adapter_ip_info_t));

    if (tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip) == 0) {
        ESP_LOGI(TAG, "IP: %d.%d.%d.%d", IP2STR(&ip.ip));
        ESP_LOGI(TAG, "MASK: %d.%d.%d.%d", IP2STR(&ip.netmask));
        ESP_LOGI(TAG, "GW: %d.%d.%d.%d", IP2STR(&ip.gw));
        return;
    }
}


void initialise_wifi(void)
{
    setup_wifi();
    for (int i = 0; i < connections.size(); ++i) {
        connect_wifi( connections[i]);

        for (int tmout = 5; tmout >0; --tmout) {
            ESP_LOGI(TAG, "Waiting for connection");
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            if (network_is_alive()) {
                break;
            }
        }
        
        if (network_is_alive()) {
            break;
        }
   } 

    if (network_is_alive()) {
        vTaskDelay(4000 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "Wifi connected");
        printNetworkInfo();
    } else {
        esp_wifi_deinit();
    }
 }

/**
@brief Check if Wi-Fi connection is alive
ToDo: Check if we have internet connectivity available

@return
    - true - yes, we are connected
    - false - no, we are not connected
*/
bool network_is_alive(void)
{
    EventBits_t uxBits = xEventGroupGetBits(wifi_event_group);
    if (uxBits & CONNECTED_BIT) {
        return true;
    } else {
        return false;
    }
}

#endif