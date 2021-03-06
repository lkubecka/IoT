#include <sys/time.h>
#include "time.hpp"

namespace kalfy
{
	namespace time
	{
		long durationBeforeNow(struct timeval *past, struct timeval *now)
		{
			struct timeval delta = _sub(now, past);
			if (delta.tv_sec < 0)
			{
				return 0;
			}
			return _toMicroSecs(&delta);
		}

		struct timeval _sub(struct timeval *a, struct timeval *b)
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

		long _toMicroSecs(struct timeval *a)
		{
			return a->tv_sec * 1000000L + a->tv_usec;
		}

		struct timeval getCurrentTime()
		{
			struct timeval now;
			gettimeofday(&now, NULL);
			return now;
		}
	}
}