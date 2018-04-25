#include <Arduino.h>
#include <SPIFFS.h>
#include <FS.h>

namespace kalfy
{
	namespace files
	{
		File openFileForUpload(const char * fileName)
		{
			{
#ifdef _DEBUG
				Serial.println("=== openFileForUpload called");
#endif
			}
			SPIFFS.begin(true);
			return SPIFFS.open(fileName, FILE_READ);
		}

		void uploadSucceeded(File file, const char * fileName)
		{
			file.close();
			if (!SPIFFS.remove(fileName))
			{
#ifdef _DEBUG
				Serial.println("=== Deleting failed");
#endif
			}
			SPIFFS.end();
		}

		void uploadFailed(File file)
		{
			file.close();
			SPIFFS.end();
		}

		void appendToFile(const char * fileName, const char * line)
		{
#ifdef _DEBUG
			Serial.println("== Appending to file");
#endif
			SPIFFS.begin(true);
			File file = SPIFFS.open(fileName, FILE_APPEND);
			if (!file)
			{
				Serial.println("== Appending failed - could not open file");
				return;
			}
			size_t size = file.println(line);
#ifdef _DEBUG
			Serial.println(size ? "== Appending succeeded" : "== Appending failed - could not print to file");
#endif
			file.flush();
			file.close();
			SPIFFS.end();
		}

		bool hasData(const char * fileName)
		{
			SPIFFS.begin(true);
			File file = SPIFFS.open(fileName, FILE_READ);
			if (!file)
			{
				return false;
			}
			bool hasData = file.peek() >= 0;

			file.close();
			SPIFFS.end();

			return hasData;
		}


		void deleteFile(const char * fileName)
		{
#ifdef _DEBUG
			Serial.println("== Deleting file");
#endif

			SPIFFS.remove(fileName);

			SPIFFS.end();
		}
	}
}