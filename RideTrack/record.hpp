#ifndef RECORD_H
#define RECORD_H

#include <FS.h>

namespace kalfy
{
	namespace record
	{
		void saveDate();
		void uploadAll(const char * apiUrl, const char * deviceId);
		void _saveDate(timeval & dateNow);
		void savePressure(int32_t pressurePa);
	}
}

#endif // !RECORD_H