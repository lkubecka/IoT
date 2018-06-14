#ifndef TIME_H
#define TIME_H

#include <sys/time.h>
#include <stdint.h>

namespace kalfy
{
	namespace time
	{
		long durationBeforeNow(struct timeval *past);
		struct timeval getCurrentTime();
		void setTime(long secondsSinceEpoch);
		unsigned long toMicroSecs(struct timeval *a);
		unsigned long toMiliSecs(struct timeval *a);
		void toMiliSecsStr(struct timeval *a, char * buffer);
		struct timeval sub(struct timeval *a, struct timeval *b);
		unsigned long getTime(void);
		void printTimeDifference(uint64_t seconds);
	}
}

#endif