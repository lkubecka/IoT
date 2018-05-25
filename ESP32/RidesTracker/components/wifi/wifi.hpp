/* 
 wifi.h - Wi-Fi setup routines

 This file is part of the ESP32 Everest Run project
 https://github.com/krzychb/esp32-everest-run

 Copyright (c) 2016 Krzysztof Budzynski <krzychb@gazeta.pl>
 This work is licensed under the Apache License, Version 2.0, January 2004
 See the file LICENSE for details.
*/

#ifndef WIFI_H
#define WIFI_H



#include "freertos/event_groups.h"
#include "Configuration.hpp"
#include <vector>
#include "string.h"

#ifdef _ESP_WIFI__
    extern EventGroupHandle_t wifi_event_group;
    extern const int CONNECTED_BIT;
    extern std::vector<Configuration> connections;
#endif

void connectWifi(void);
bool connectWifiManual(void);
void disconnectWifi(void);



#endif
