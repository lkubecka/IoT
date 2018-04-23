#ifndef TIME_H
#define TIME_H

#include <sys/time.h>

namespace kalfy
{
namespace time
{
long durationBeforeNow(struct timeval *past, struct timeval *now);
struct timeval _sub(struct timeval *a, struct timeval *b);
long _toMicroSecs(struct timeval *a);
struct timeval getCurrentTime();
}
}

#endif