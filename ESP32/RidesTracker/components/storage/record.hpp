#ifndef RECORD_H
#define RECORD_H

#include <FS.h>
#include <sys/time.h>

extern const char* ODOCYCLE_SERVER;
extern const char* ODOCYCLE_ID;
extern const char* ODOCYCLE_TOKEN;
extern const char* ODOCYCLE_CERT;

namespace kalfy
{
	namespace record
	{
		void saveRevolution(timeval &timestamp);
		void uploadAll(const char * apiUrl, const char * deviceId, const char* token, const char * ca_cert);
		void _saveDate(timeval & dateNow);
		void savePressure(int32_t pressurePa);
		bool hasData();
		File openRecordForUpload();
		void uploadSucceeded(File file);
		void uploadFailed(File file);
		void printAll();
		void clear();
	}
}

#endif // !RECORD_H