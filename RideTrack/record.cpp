﻿#include <Arduino.h>
#include <SPIFFS.h>
#include <FS.h>
#include <HTTPClient.h>
#include <sys/time.h>
#include "record.hpp"
#include "files.hpp"
#include "time.hpp"

namespace kalfy
{
	namespace record
	{
		const char* DESTINATION_FILE = "/records.txt";

		void saveRevolution(timeval &now)
		{
			//timeval now = kalfy::time::getCurrentTime();
			char buffer[32];
			snprintf(buffer, sizeof(buffer), "t%ld|%ld", now.tv_sec, now.tv_usec);
			Serial.println(buffer);
			kalfy::files::appendToFile(DESTINATION_FILE, buffer);
		}

		void savePressure(int32_t pressurePa)
		{
			char buffer[16];
			snprintf(buffer, sizeof(buffer), "p%d", pressurePa);
			Serial.println(buffer);
			kalfy::files::appendToFile(DESTINATION_FILE, buffer);
		}

		bool hasData() {
			return kalfy::files::hasData(DESTINATION_FILE);
		}

		File openRecordForUpload()
		{
			return kalfy::files::openFileForUpload(DESTINATION_FILE);
		}

		void uploadSucceeded(File file)
		{
			kalfy::files::uploadSucceeded(file, DESTINATION_FILE);
		}

		void uploadFailed(File file)
		{
			kalfy::files::uploadFailed(file);
		}

		void uploadAll(const char * apiUrl, const char * deviceId, const char * token, const char * ca_cert)
		{
#ifdef _DEBUG
			Serial.println("=== uploadAll called");
#endif
			if (apiUrl == nullptr || deviceId == nullptr)
			{
#ifdef _DEBUG
				Serial.println("Some of the arguments is null!");
#endif
				return;
			}

			File file = kalfy::files::openFileForUpload(DESTINATION_FILE);
			if (!file || file.size() == 0)
			{
				// the HTTPClient API has some issue with zero-size files, crashes, so we must check for an empty file
#ifdef _DEBUG
				Serial.println("=== Nothing to send");
#endif
				return;
			}

			HTTPClient http;
			http.begin(String(apiUrl) + deviceId, ca_cert);
			http.addHeader("Authorization", "Bearer " + String(token));
			http.addHeader("Content-Type", "application/form-data");
			int httpResponseCode = http.sendRequest("POST", &file, file.size());
			if (httpResponseCode == 201)
			{
#ifdef _DEBUG
				Serial.println("== Data sent successfully");
#endif
				kalfy::files::uploadSucceeded(file, DESTINATION_FILE);
			}
			else
			{

				Serial.println("== Sending to server failed");
				String response = http.getString();
				Serial.println("HTTP response code:");
				Serial.println(httpResponseCode);
				Serial.println("HTTP response body:");
				Serial.println(response);

				kalfy::files::uploadFailed(file);
			}
			http.end();
			Serial.println("== Upload complete");
		}


		void printAll()
		{
#ifdef _DEBUG
			Serial.println("=== printAll called");
#endif

			File file = kalfy::files::openFileForUpload(DESTINATION_FILE);
			if (!file || file.size() == 0)
			{
				// the HTTPClient API has some issue with zero-size files, crashes, so we must check for an empty file
#ifdef _DEBUG
				Serial.println("=== Nothing to print");
#endif
				return;
			}

			String indexHTML;
			unsigned int cnt = 0;

			while (file.available()) {
				Serial.write(file.read());
				cnt++;
			}
			Serial.println(cnt);
			uploadFailed(file);
			
		}
		void clear() {
			kalfy::files::deleteFile(DESTINATION_FILE);
		}
	}


}
