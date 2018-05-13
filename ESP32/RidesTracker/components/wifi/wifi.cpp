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

#include "wifi.hpp"
#include "Configuration.hpp"
#include <vector>
#include "string.h"

static const char* TAG = "Wi-Fi";

#define WIFI_SSID "NAHATCH1"
#define WIFI_PASS "nahatch123"

std::vector<Configuration> connections = {  
    Configuration("NATATCH", "nahatch123"),
    Configuration("sde-guest", "4Our6uest"),
    Configuration("NATATCH", "nahatch123")
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

    wifi_config_t wifi_config;
    strncpy((char *)wifi_config.sta.ssid, connection.getSSID(), sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, connection.getPassword(), sizeof(wifi_config.sta.password));

    ESP_LOGI(TAG, "Setting SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, 1000 / portTICK_PERIOD_MS);

}


void printNetworkInfo(void) {
    tcpip_adapter_ip_info_t ip;
    memset(&ip, 0, sizeof(tcpip_adapter_ip_info_t));
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    if (tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip) == 0) {
        ESP_LOGI(TAG, "~~~~~~~~~~~");
        ESP_LOGI(TAG, "IP: %d.%d.%d.%d", IP2STR(&ip.ip));
        ESP_LOGI(TAG, "MASK: %d.%d.%d.%d", IP2STR(&ip.netmask));
        ESP_LOGI(TAG, "GW: %d.%d.%d.%d", IP2STR(&ip.gw));
        ESP_LOGI(TAG, "~~~~~~~~~~~");
        return;
    }
}


void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

    ESP_LOGI(TAG, "Initialising");

    // wifi_config_t wifi_config = {
    //     .sta = {
    //         .ssid = WIFI_SSID,
    //         .password = WIFI_PASS
    //     }
    // };

    wifi_config_t wifi_config;
    wifi_config.sta.ssid[0] = 'N';
    wifi_config.sta.ssid[1] = 'A';
    wifi_config.sta.ssid[2] = 'H';
    wifi_config.sta.ssid[3] = 'A';
    wifi_config.sta.ssid[4] = 'T';
    wifi_config.sta.ssid[5] = 'C';
    wifi_config.sta.ssid[6] = 'H';
    wifi_config.sta.ssid[7] = '\0';
    
    wifi_config.sta.password[0] = 'n';
    wifi_config.sta.password[1] = 'a';
    wifi_config.sta.password[2] = 'h';
    wifi_config.sta.password[3] = 'a';
    wifi_config.sta.password[4] = 't';
    wifi_config.sta.password[5] = 'c';
    wifi_config.sta.password[6] = 'h';
    wifi_config.sta.password[7] = '1';
    wifi_config.sta.password[8] = '2';
    wifi_config.sta.password[9] = '3';
    wifi_config.sta.password[10] = '\0';

    ESP_LOGI(TAG, "Setting SSID: %s and password: %s", wifi_config.sta.ssid, wifi_config.sta.password);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, 1000 / portTICK_PERIOD_MS);

    if (network_is_alive()) {
        ESP_LOGI(TAG, "Wifi connected");    
        printNetworkInfo();
    }
}

void initialise_wifi2(void)
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
