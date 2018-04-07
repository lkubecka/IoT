#include "time.h"

void setup() {
    // TODO: bude to ještě potřeba nejdříve připojit na Wi-Fi, podle mě to získává čas ze SNTP
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
        Serial.println("Failed to obtain time");
        return;
    }
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

    // odkazy na issues:
    // https://github.com/espressif/arduino-esp32/issues/231
    // https://github.com/espressif/arduino-esp32/issues/403
}

void loop() {

}