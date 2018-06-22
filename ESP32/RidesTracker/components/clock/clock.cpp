
#include "clock.hpp"
#include <stdint.h>
#include <ctime>
#include <sys/time.h>
#include <sys/cdefs.h>
#include <string.h>
#include "esp_log.h"
#include "Arduino.h"
#include "time.hpp"
#include "nvs_flash.h"

using namespace kalfy;

constexpr char Clock::TAG[];
constexpr char Clock::ntpServer[];
constexpr long  Clock::gmtOffset_sec;
constexpr int   Clock::daylightOffset_sec;
constexpr uint64_t Clock::MAX_IDLE_TIME_US;
constexpr uint64_t Clock::POST_DATA_PERIOD_SEC;

static RTC_DATA_ATTR struct timeval RTCTime;

Clock::Clock() {
    lastActivityTime = { 0UL, 0UL };
}

void Clock::setLastActivity(struct timeval time) {
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
    printTime(&timeWhenPost, "Loaded timeWhenPost: %s");
}

bool Clock::isLastActivityTimeout(void) {
    struct timeval now = kalfy::time::getCurrentTime();
    struct timeval delta = kalfy::time::sub(&now, &lastActivityTime);
    return (kalfy::time::toMicroSecs(&delta) > MAX_IDLE_TIME_US);
}

void Clock::saveTimeWhenPost(void) {
    gettimeofday(&timeWhenPost, NULL);
    nvs_flash_init();
    vTaskDelay(1000 / portTICK_RATE_MS);
    preferences.begin("RidezTracker", false);
    preferences.putBytes("timeWhenPost", &timeWhenPost, sizeof(timeWhenPost));
    preferences.end();
    printTime(&timeWhenPost, "Saved timeWhenPost: %s");
}

bool Clock::isTimeToSendData(void) {
    loadTimeWhenPost();
    struct timeval now = kalfy::time::getCurrentTime();
    printTime(&now, "Current time %s");
    struct timeval delta = kalfy::time::sub(&now, &timeWhenPost);
    char timeDiffStr[40];
    kalfy::time::getTimeDifferenceStr(timeDiffStr, delta.tv_sec);
    ESP_LOGI(TAG, "Time spent from last post: %s", timeDiffStr);
    ESP_LOGI(TAG, "Time spent from last post: %ld s", delta.tv_sec);
    ESP_LOGI(TAG, "Time spent from last post direct: %ld s", now.tv_sec - timeWhenPost.tv_sec);

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
    RTCTime = sleepEnterTime;
	preferences.begin("RidezTracker", false);
	preferences.putBytes("sleepEnterTime", &sleepEnterTime, sizeof(sleepEnterTime));
	preferences.end();
    printTime(&sleepEnterTime, "Saved sleepEnterTime: %s");
}


void Clock::loadSleepEnterTime(void) {
    nvs_flash_init();
    vTaskDelay(1000 / portTICK_RATE_MS);
    preferences.begin("RidezTracker", false);
    preferences.getBytes("sleepEnterTime", &sleepEnterTime, sizeof(sleepEnterTime));
    preferences.end();
    printTime(&sleepEnterTime, "Loaded sleepEnterTime: %s");
    printTime(&RTCTime, "RTC time: %s");
}

void Clock::updateActivityTime(void) {
    lastActivityTime = kalfy::time::getCurrentTime();
}

time_t Clock::getSecondsSpentInSleep(void) {
    loadSleepEnterTime();
    struct timeval now = kalfy::time::getCurrentTime();
    struct timeval delta = kalfy::time::sub(&now, &sleepEnterTime);
    char timeDiffStr[40];
    kalfy::time::getTimeDifferenceStr(timeDiffStr, delta.tv_sec);
    printTime(&now, "Current time: %s");
    printTime(&sleepEnterTime, "sleepEnterTime: %s");
    ESP_LOGI(TAG, "Time spent in deep sleep: %s\n", timeDiffStr);

    return delta.tv_sec;
}


void Clock::setLastKnownTime(void) {
    nvs_flash_init();
    vTaskDelay(1000 / portTICK_RATE_MS);

    updateNumberOfResets();
    
	struct timeval now = kalfy::time::getCurrentTime();
    printTime(&now, "Current GMT time: %s");

	if (now.tv_sec < 10000) {
        ESP_LOGI(TAG, "Current time lost, loading sleepEnterTime");
        setTime(&sleepEnterTime);	
        printTime(&now, "Current GMT time: %s");	
        printCurrentLocalTime();
	}
    updateActivityTime();
}

void Clock::updateTimeFromNTP(void) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    printCurrentLocalTime();
    saveSleepEnterTime();
}

void Clock::setTimeZone(long offset_sec, int daylight_sec) {
    char cst[16] = {0};
    char cdt[16] = "DST";
    char tz[32] = {0};

    if(offset_sec % 3600){
        sprintf(cst, "UTC%ld:%02ld:%02ld", offset_sec / 3600, abs((offset_sec % 3600) / 60), abs(offset_sec % 60));
    } else {
        sprintf(cst, "UTC%ld", offset_sec / 3600);
    }
    if(daylight_sec != 3600){
        long tz_dst = offset_sec - daylight_sec;
        if(tz_dst % 3600){
            sprintf(cdt, "DST%ld:%02ld:%02ld", tz_dst / 3600, abs((tz_dst % 3600) / 60), abs(tz_dst % 60));
        } else {
            sprintf(cdt, "DST%ld", tz_dst / 3600);
        }
    }
    sprintf(tz, "%s%s", cst, cdt);
    ESP_LOGI(TAG, "Timezone: %s", tz);
    setenv("TZ", tz, 1);
    tzset();
}

void Clock::setTime(struct timeval* now) {
    settimeofday(now, NULL);
    setTimeZone(-gmtOffset_sec, daylightOffset_sec);
}

void Clock::printTime(struct timeval* timeVal, const char * strFormat ) {
    struct tm timeinfo;
    char strftime_buf[64];
    char time_buf[100];

    gmtime_r(&timeVal->tv_sec, &timeinfo);
    
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    sprintf(time_buf, strFormat, strftime_buf);
    ESP_LOGI(TAG, "%s", time_buf);
}

void Clock::printCurrentLocalTime(void) {
    struct tm timeinfo;
    char strftime_buf[64];
    time_t now;
    
    ::time(&now);
    setTimeZone(-gmtOffset_sec, daylightOffset_sec);
    localtime_r(&now, &timeinfo);
    
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current local date/time is: %s", strftime_buf);
    // struct tm timeinfo;
    // char strftime_buf[64];

    // if (!getLocalTime(&timeinfo, 1000)) {
    //     ESP_LOGI(TAG, "Failed to obtain time");
    //     return;
    // }
    
    // strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    // ESP_LOGI(TAG, "The current GMT date/time is: %s", strftime_buf);

    // setTimeZone(gmtOffset_sec, daylightOffset_sec);
    // strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    // ESP_LOGI(TAG, "The current date/time in Prague is: %s", strftime_buf);
}