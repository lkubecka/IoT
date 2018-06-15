#include "clock.hpp"
#include <stdint.h>
#include <sys/time.h>
#include "esp_log.h"
#include "Arduino.h"
#include "time.hpp"
#include "nvs_flash.h"

using namespace kalfy::time;

constexpr char Clock::TAG[];
constexpr char Clock::ntpServer[];
constexpr long  Clock::gmtOffset_sec;
constexpr int   Clock::daylightOffset_sec;
constexpr uint64_t Clock::MAX_IDLE_TIME_US;
constexpr uint64_t Clock::POST_DATA_PERIOD_SEC;

Clock::Clock() {
//    lastActivityTime = { 0UL, 0UL };
//    timeWhenPost = { 0UL, 0UL };
//    sleepEnterTime = { 0UL, 0UL };
}

void Clock::getLastActivity(struct timeval time) {
    lastActivityTime = time;
}

struct timeval Clock::getLastActivity(void) {
    return lastActivityTime;
}

void Clock::loadTimeWhenPost(void) {
    nvs_flash_init();
    vTaskDelay(1000 / portTICK_RATE_MS);
    preferences.begin("RidezTracker", false);
    preferences.getBytes("timeWhenPost", &timeWhenPost, sizeof(timeWhenPost));
    preferences.end();
}

bool Clock::isLastActivityTimeout(void) {
    struct timeval now = getCurrentTime();
    struct timeval delta = sub(&now, &lastActivityTime);
    return (toMicroSecs(&delta) > MAX_IDLE_TIME_US);
}

void Clock::saveTimeWhenPost(void) {
    gettimeofday(&timeWhenPost, NULL);
    nvs_flash_init();
    vTaskDelay(1000 / portTICK_RATE_MS);
    preferences.begin("RidezTracker", false);
    preferences.putBytes("timeWhenPost", &timeWhenPost, sizeof(timeWhenPost));
    preferences.end();
}

bool Clock::isTimeToSendData(void) {
    loadTimeWhenPost();
    struct timeval now = getCurrentTime();
    struct timeval delta = sub(&now, &timeWhenPost);
    ESP_LOGI(TAG, "Time since last post: %s\n", getTimeDifferenceStr(delta.tv_sec));

    return (delta.tv_sec > POST_DATA_PERIOD_SEC );
}

void Clock::updateNumberOfResets(void) {
    preferences.begin("RidezTracker", false);
	unsigned int resetTimes = preferences.getUInt("resetTimes", 0);
	preferences.putUInt("resetTimes", ++resetTimes);
    ESP_LOGI( TAG, "Number of restart times: %d\n", resetTimes);
}

void Clock::saveSleepEnterTime(void) {
    gettimeofday(&sleepEnterTime, NULL);
	preferences.begin("RidezTracker", false);
	preferences.putBytes("sleepEnterTime", &sleepEnterTime, sizeof(sleepEnterTime));
	preferences.end();
}


void Clock::loadSleepEnterTime(void) {
    nvs_flash_init();
    vTaskDelay(1000 / portTICK_RATE_MS);
    preferences.begin("RidezTracker", false);
    preferences.getBytes("sleepEnterTime", &sleepEnterTime, sizeof(sleepEnterTime));
    preferences.end();
}

void Clock::updateActivityTime(void) {
    lastActivityTime = getCurrentTime();
}


void Clock::setLastKnownTime(void) {
    nvs_flash_init();
    vTaskDelay(1000 / portTICK_RATE_MS);

    updateNumberOfResets();
    loadSleepEnterTime();
    struct timeval now = getCurrentTime();
    struct timeval delta = sub(&now, &sleepEnterTime);
    ESP_LOGI(TAG, "Time spent in deep sleep: %s\n", getTimeDifferenceStr(delta.tv_sec));
    
    printLocalTime();
	if (now.tv_sec < 10000) {
		ESP_LOGI( TAG, "Time lost, loading last known time: %ld\n", now.tv_sec);
        setLocalTime(&now);		
        printLocalTime();
	}
    updateActivityTime();
}

void Clock::updateTimeFromNTP(void) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    printLocalTime();
    saveSleepEnterTime();
}

void Clock::setLocalTime(struct timeval* now) {
    struct timezone tz;
    tz.tz_minuteswest = gmtOffset_sec/60;
    tz.tz_dsttime = daylightOffset_sec/60;
    settimeofday(now, &tz);
}

void Clock::printLocalTime(void)
{
    struct tm * timeinfo;
    if (!getLocalTime(timeinfo)) {
        ESP_LOGI(TAG, "Failed to obtain time");
        return;
    }
    ESP_LOGI(TAG, "Current local time and date: %s", asctime(timeinfo));
}