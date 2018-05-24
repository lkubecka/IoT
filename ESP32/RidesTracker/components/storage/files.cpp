#include <Arduino.h>
#include <SPIFFS.h>
#include <FS.h>

#ifdef ARDUINO_ARCH_ESP32
#include "esp32-hal-log.h"
#endif

namespace kalfy
{
	namespace files
	{
		static const char* TAG = "files";

		File openFileForUpload(const char * fileName)
		{
			ESP_LOGI(TAG, "=== openFileForUpload called");
			SPIFFS.begin(true);
			return SPIFFS.open(fileName, FILE_READ);
		}

		void uploadSucceeded(File file, const char * fileName)
		{
			file.close();
			if (!SPIFFS.remove(fileName))
			{
				ESP_LOGI(TAG, "=== Deleting failed");
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
			ESP_LOGI(TAG, "== Appending to file");

			SPIFFS.begin(true);
			File file = SPIFFS.open(fileName, FILE_APPEND);
			if (!file)
			{
				ESP_LOGI(TAG, "== Appending failed - could not open file");
				return;
			}
			size_t size = file.println(line);
			ESP_LOGI(TAG, "%s", size ? "== Appending succeeded" : "== Appending failed - could not print to file");			
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
			SPIFFS.begin(true);
			ESP_LOGI(TAG, "== Deleting file");

			SPIFFS.remove(fileName);

			SPIFFS.end();
		}
	}
}