#ifndef TIME_H
#define TIME_H

#include <sys/time.h>

namespace kalfy
{
	namespace time
	{
		long durationBeforeNow(struct timeval *past);
		struct timeval getCurrentTime();
		void setTime(long secondsSinceEpoch);
		long toMicroSecs(struct timeval *a);
		long toMiliSecs(struct timeval *a);
		struct timeval sub(struct timeval *a, struct timeval *b);
	}
}

#endif