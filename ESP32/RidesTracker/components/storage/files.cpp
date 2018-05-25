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

		void initSPIFFS(void) {
			SPIFFS.begin(true);
		}
		void detachSPIFFS(void) {
			SPIFFS.end();
		}

		File openFileForUpload(const char * fileName)
		{
			ESP_LOGI(TAG, "=== openFileForUpload called");
			
			return SPIFFS.open(fileName, FILE_READ);
		}

		void uploadSucceeded(File file, const char * fileName)
		{
			file.close();
			if (!SPIFFS.remove(fileName))
			{
				ESP_LOGI(TAG, "=== Deleting failed");
			}
		}

		void uploadFailed(File file)
		{
			file.close();
		}

		void appendToFile(const char * fileName, const char * line)
		{
			ESP_LOGI(TAG, "== Appending to file");

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
		}

		bool hasData(const char * fileName)
		{
			File file = SPIFFS.open(fileName, FILE_READ);
			if (!file)
			{
				return false;
			}
			bool hasData = file.peek() >= 0;

			file.close();

			return hasData;
		}


		void deleteFile(const char * fileName)
		{
			ESP_LOGI(TAG, "== Deleting file");

			SPIFFS.remove(fileName);
		}
	}
}