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
		extern const char* DESTINATION_FILE;
		extern const char* TEST_FILE;

		void saveRevolution(timeval &timestamp, const char * filename);
		void uploadAll(const char * apiUrl, const char * deviceId, const char* token, const char * ca_cert);
		void uploadFile(const char * apiUrl, const char * deviceId, const char * token, const char * ca_cert, const char * filename);
		void uploadMultipartFile(const char * filename);
		void _saveDate(timeval & dateNow);
		void savePressure(int32_t pressurePa, const char * filename);
		bool hasData(const char * filename);
		File openRecordForUpload(const char * filename);
		void uploadSucceeded(File file, const char * filename);
		void uploadFailed(File file, const char * filename);
		void printAll(const char * filename);
		void clear(const char * filename);
		void createTestFile(const char * filename);
	}
}

#endif // !RECORD_H