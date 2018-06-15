#include <sys/cdefs.h>
#include <string.h>
#include <ctime>
#include <sys/time.h>
#include "time.hpp"
#include "esp_log.h"
#include "Arduino.h"

namespace kalfy
{
	namespace time
	{
		static const char* TAG = "Time";

		long durationBeforeNow(struct timeval *past)
		{
			struct timeval now;
			gettimeofday(&now, NULL);
			struct timeval delta = sub(&now, past);
			if (delta.tv_sec < 0L)
			{
				return 0;
			}
			return toMicroSecs(&delta);
		}

		struct timeval sub(struct timeval *a, struct timeval *b)
		{
			struct timeval result;
			result.tv_sec = a->tv_sec - b->tv_sec;
			result.tv_usec = a->tv_usec - b->tv_usec;
			if (a->tv_usec < b->tv_usec)
			{
				result.tv_sec -= 1;
				result.tv_usec += 1000000L;
			}
			return result;
		}

		unsigned long toMicroSecs(struct timeval *a)
		{
			return a->tv_sec * 1000000L + a->tv_usec;
		}

		unsigned long toMiliSecs(struct timeval *a) {
			unsigned long sec = a->tv_sec;
			unsigned long usec = a->tv_usec;
			return (unsigned long)(sec * 1000UL + usec / 1000UL);
		}

		void toMiliSecsStr(struct timeval *a, char *buffer) {
			int milli = a->tv_usec / 1000UL;
			sprintf(buffer, "%ld%.3d", a->tv_sec, milli);
			ESP_LOGI(TAG, "Timestamp ms %s", buffer);
			ESP_LOGI(TAG, "Timestamp sec %ld", a->tv_sec);
			ESP_LOGI(TAG, "Timestamp usec %ld", a->tv_usec);
			ESP_LOGI(TAG, "Timestamp ms %d", milli);
		}



		struct timeval getCurrentTime()
		{
			struct timeval now;
			gettimeofday(&now, NULL);
			return now;
		}

		void setTime(long secondsSinceEpoch) {
			struct timeval time;
			struct timezone timezone;
			time.tv_sec = secondsSinceEpoch;
			time.tv_usec = 0;
			timezone.tz_dsttime = 0;
			timezone.tz_minuteswest = 0;
			settimeofday(&time, &timezone);
		}

		unsigned long getTime(void) {
			long now;
			std::time(&now);

			return now;
		}

		char* getTimeDifferenceStr(uint64_t seconds) {
			char buffer[100];
			int days = seconds / (24 * 3600);
			seconds = seconds % (24 * 3600);
			int hours = seconds / 3600;
			seconds %= 3600;
			int min = seconds / 60 ;
			seconds %= 60;
			int sec = seconds;

			sprintf(buffer,  "%02d days, %02d:%02d:%02d [HH:MM:SS]", days, hours, min, sec);
			return buffer;
		}
	}
}