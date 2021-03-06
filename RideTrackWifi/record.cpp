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
		struct buffer_item_t {
			uint64_t id;
			double dist;
			timeval timestamp;
		};
		const char* DESTINATION_FILE = "/records.txt";

		void saveRevolution(uint64_t numberOfRevolutions, float_t wheelCircumference)
		{
			buffer_item_t bufferItem;
			bufferItem.timestamp = kalfy::time::getCurrentTime();
			bufferItem.dist = numberOfRevolutions * wheelCircumference;
			bufferItem.id = numberOfRevolutions;

			char bfr[32];
			snprintf(bfr, sizeof(bfr), "#%llu:d%f,t%ld|%ld", bufferItem.id, bufferItem.dist, bufferItem.timestamp.tv_sec, bufferItem.timestamp.tv_usec);
			Serial.println(bfr);

			kalfy::files::appendToFile(DESTINATION_FILE, bfr);
		}

		void savePressure(int32_t pressurePa)
		{
			char buffer[16];
			snprintf(buffer, sizeof(buffer), "p%d", pressurePa);
			Serial.println(buffer);
			kalfy::files::appendToFile(DESTINATION_FILE, buffer);
		}

		void uploadAll(const char* apiUrl, const char* deviceId)
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
			http.begin(String(apiUrl) + "/records?deviceId=" + deviceId);
			http.addHeader("Content-Type", "text/plain");
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
#ifdef _DEBUG
				Serial.println("== Sending to server failed");
				String response = http.getString();
				Serial.println("HTTP response code:");
				Serial.println(httpResponseCode);
				Serial.println("HTTP response body:");
				Serial.println(response);
#endif
				kalfy::files::uploadFailed(file);
			}
			http.end();
			Serial.println("== Upload complete");
		}
	}
}
