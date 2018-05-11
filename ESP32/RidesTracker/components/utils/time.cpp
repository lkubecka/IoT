#include <sys/cdefs.h>
#include <sys/time.h>
#include "time.hpp"

namespace kalfy
{
	namespace time
	{
		long durationBeforeNow(struct timeval *past)
		{
			struct timeval now;
			gettimeofday(&now, NULL);
			struct timeval delta = sub(&now, past);
			if (delta.tv_sec < 0)
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
				result.tv_usec += 1000000;
			}
			return result;
		}

		long toMicroSecs(struct timeval *a)
		{
			return a->tv_sec * 1000000L + a->tv_usec;
		}

		long toMiliSecs(struct timeval *a) {
			return a->tv_sec * 1000 + a->tv_usec / 1000;
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
	}
}