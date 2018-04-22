#ifndef RECORD_H
#define RECORD_H

#include <FS.h>

namespace kalfy
{
	namespace record
	{
		void saveRevolution(uint64_t numberOfRevolutions, float_t wheelCircumference);
		void uploadAll(const char * apiUrl, const char * deviceId);
		//void _saveDate(timeval & dateNow);
		void savePressure(int32_t pressurePa);
	}
}

#endif // !RECORD_H