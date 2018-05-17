﻿#include <Arduino.h>
#include <SPIFFS.h>
#include <FS.h>
#include <HTTPClient.h>
#include <sys/time.h>
#include "record.hpp"
#include "files.hpp"
#include "time.hpp"

#ifdef ARDUINO_ARCH_ESP32
#include "esp32-hal-log.h"
#endif


const char* ODOCYCLE_SERVER = "https://ridestracker.azurewebsites.net/api/v1/Records/";
const char* ODOCYCLE_ID = "4f931a53-6e1b-4e85-bbda-7c71d9f8f2b9";
const char* ODOCYCLE_TOKEN = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiI5YmQwYzhmMC05ZTI0LTQxZTgtYjkwNi02ZDI3MGFjNGFkN2UiLCJqdGkiOiJiNDg5ODE1Yy1iOWU2LTRjNzUtOTNmZS0wNDE4MTEwYzk5NDciLCJleHAiOjE1MjczNDEzMjMsImlzcyI6ImF6dXJlLWRldiIsImF1ZCI6ImF6dXJlLWRldiJ9.FjuVmDSP8HGJttu7hC_T6rRoCLUDYeadn_cZdMNITb0";

const char* ODOCYCLE_CERT = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIFtDCCBJygAwIBAgIQC2qzsD6xqfbEYJJqqM3+szANBgkqhkiG9w0BAQsFADBa\n" \
"MQswCQYDVQQGEwJJRTESMBAGA1UEChMJQmFsdGltb3JlMRMwEQYDVQQLEwpDeWJl\n" \
"clRydXN0MSIwIAYDVQQDExlCYWx0aW1vcmUgQ3liZXJUcnVzdCBSb290MB4XDTE2\n" \
"MDUyMDEyNTIzOFoXDTI0MDUyMDEyNTIzOFowgYsxCzAJBgNVBAYTAlVTMRMwEQYD\n" \
"VQQIEwpXYXNoaW5ndG9uMRAwDgYDVQQHEwdSZWRtb25kMR4wHAYDVQQKExVNaWNy\n" \
"b3NvZnQgQ29ycG9yYXRpb24xFTATBgNVBAsTDE1pY3Jvc29mdCBJVDEeMBwGA1UE\n" \
"AxMVTWljcm9zb2Z0IElUIFRMUyBDQSA0MIICIjANBgkqhkiG9w0BAQEFAAOCAg8A\n" \
"MIICCgKCAgEAq+XrXaNrOZ71NIgSux1SJl19CQvGeY6rtw7fGbLd7g/27vRW5Ebi\n" \
"kg/iZwvjHHGk1EFztMuZFo6/d32wrx5s7XEuwwh3Sl6Sruxa0EiB0MXpoPV6jx6N\n" \
"XtOtksDaxpE1MSC5OQTNECo8lx0AnpkYGAnPS5fkyfwA8AxanTboskDBSqyEKKo9\n" \
"Rhgrp4qs9K9LqH5JQsdiIMDmpztd65Afu4rYnJDjOrFswpTOPjJry3GzQS65xeFd\n" \
"2FkngvvhSA1+6ATx+QEnQfqUWn3FMLu2utcRm4j6AcxuS5K5+Hg8y5xomhZmiNCT\n" \
"sCqDLpcRHX6BIGHksLmbnG5TlZUixtm9dRC62XWMPD8d0Jb4M0V7ex9UM+VIl6cF\n" \
"JKLb0dyVriAqfZaJSHuSetAksd5IEfdnPLTf+Fhg9U97NGjm/awmCLbzLEPbT8QW\n" \
"0JsMcYexB2uG3Y+gsftm2tjL6fLwZeWO2BzqL7otZPFe0BtQsgyFSs87yC4qanWM\n" \
"wK5c2enAfH182pzjvUqwYAeCK31dyBCvLmKM3Jr94dm5WUiXQhrDUIELH4Mia+Sb\n" \
"vCkigv2AUVx1Xw41wt1/L3pnnz2OW4y7r530zAz7qB+dIcHz51IaXc4UV21QuEnu\n" \
"sQsn0uJpJxJuxsAmPuekKxuLUzgG+hqHOuBLf5kWTlk9WWnxcadlZRsCAwEAAaOC\n" \
"AUIwggE+MB0GA1UdDgQWBBR6e4zBz+egyhzUa/r74TPDDxqinTAfBgNVHSMEGDAW\n" \
"gBTlnVkwgkdYzKz6CFQ2hns6tQRN8DASBgNVHRMBAf8ECDAGAQH/AgEAMA4GA1Ud\n" \
"DwEB/wQEAwIBhjAnBgNVHSUEIDAeBggrBgEFBQcDAQYIKwYBBQUHAwIGCCsGAQUF\n" \
"BwMJMDQGCCsGAQUFBwEBBCgwJjAkBggrBgEFBQcwAYYYaHR0cDovL29jc3AuZGln\n" \
"aWNlcnQuY29tMDoGA1UdHwQzMDEwL6AtoCuGKWh0dHA6Ly9jcmwzLmRpZ2ljZXJ0\n" \
"LmNvbS9PbW5pcm9vdDIwMjUuY3JsMD0GA1UdIAQ2MDQwMgYEVR0gADAqMCgGCCsG\n" \
"AQUFBwIBFhxodHRwczovL3d3dy5kaWdpY2VydC5jb20vQ1BTMA0GCSqGSIb3DQEB\n" \
"CwUAA4IBAQAR/nIGOiEKN27I9SkiAmKeRQ7t+gaf77+eJDUX/jmIsrsB4Xjf0YuX\n" \
"/bd38YpyT0k66LMp13SH5LnzF2CHiJJVgr3ZfRNIfwaQOolm552W95XNYA/X4cr2\n" \
"du76mzVIoZh90pMqT4EWx6iWu9El86ZvUNoAmyqo9DUA4/0sO+3lFZt/Fg/Hjsk2\n" \
"IJTwHQG5ElBQmYHgKEIsjnj/7cae1eTK6aCqs0hPpF/kixj/EwItkBE2GGYoOiKa\n" \
"3pXxWe6fbSoXdZNQwwUS1d5ktLa829d2Wf6l1uVW4f5GXDuK+OwO++8SkJHOIBKB\n" \
"ujxS43/jQPQMQSBmhxjaMmng9tyPKPK9\n" \
"-----END CERTIFICATE-----\n";

namespace kalfy
{
	namespace record
	{
		static const char* TAG = "record";

		const char* DESTINATION_FILE = "/records.txt";

		void saveRevolution(timeval &now)
		{
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

			ESP_LOGI(TAG, "=== uploadAll called");
			if (apiUrl == nullptr || deviceId == nullptr)
			{
				ESP_LOGI(TAG,"Some of the arguments is null!");
				return;
			}

			File file = kalfy::files::openFileForUpload(DESTINATION_FILE);
			if (!file || file.size() == 0)
			{
				// the HTTPClient API has some issue with zero-size files, crashes, so we must check for an empty file
				ESP_LOGI(TAG, "=== Nothing to send");
				return;
			}

			HTTPClient http;
			http.begin(String(apiUrl) + deviceId, ca_cert);
			http.addHeader("Authorization", "Bearer " + String(token));
			http.addHeader("Content-Type", "application/form-data");
			int httpResponseCode = http.sendRequest("POST", &file, file.size());
			if (httpResponseCode == 201)
			{
				ESP_LOGI(TAG, "== Data sent successfully");
				kalfy::files::uploadSucceeded(file, DESTINATION_FILE);
			}
			else
			{

				ESP_LOGI(TAG, "== Sending to server failed");
				String response = http.getString();
				ESP_LOGI(TAG, "HTTP response code:");
				ESP_LOGI(TAG, "%d", httpResponseCode);
				ESP_LOGI(TAG, "HTTP response body:");
				ESP_LOGI(TAG, "%s", response);

				kalfy::files::uploadFailed(file);
			}
			http.end();
			ESP_LOGI(TAG, "== Upload complete");
		}


		void printAll()
		{

			ESP_LOGI(TAG,"=== printAll called");

			File file = kalfy::files::openFileForUpload(DESTINATION_FILE);
			if (!file || file.size() == 0)
			{
				// the HTTPClient API has some issue with zero-size files, crashes, so we must check for an empty file
				ESP_LOGI(TAG, "=== Nothing to print");
				return;
			}

			unsigned int cnt = 0;

			while (file.available()) {
				Serial.write(file.read());
				cnt++;
			}
			ESP_LOGI(TAG, "%d",cnt);
			uploadFailed(file);
			
		}
		void clear() {
			kalfy::files::deleteFile(DESTINATION_FILE);
		}

		
		void uploadTest(const char * apiUrl, const char * deviceId, const char * token, const char * ca_cert)
		{
			const char* TEST_FILE = "/test.txt";
			const int32_t presure = 101250; 
			struct timeval now = kalfy::time::getCurrentTime();

			kalfy::files::openFileForUpload(TEST_FILE);
			saveRevolution(now);
			savePressure(presure);
			now = kalfy::time::getCurrentTime();
			saveRevolution(now);
			savePressure(presure);



			ESP_LOGI(TAG, "=== uploadAll called");
			if (apiUrl == nullptr || deviceId == nullptr)
			{
				ESP_LOGI(TAG,"Some of the arguments is null!");
				return;
			}

			File file = kalfy::files::openFileForUpload(TEST_FILE);
			if (!file || file.size() == 0)
			{
				// the HTTPClient API has some issue with zero-size files, crashes, so we must check for an empty file
				ESP_LOGI(TAG, "=== Nothing to send");
				return;
			}

			HTTPClient http;
			http.begin(String(apiUrl) + deviceId, ca_cert);
			http.addHeader("Authorization", "Bearer " + String(token));
			http.addHeader("Content-Type", "application/form-data");
			int httpResponseCode = http.sendRequest("POST", &file, file.size());
			if (httpResponseCode == 201)
			{
				ESP_LOGI(TAG, "== Data sent successfully");
				kalfy::files::uploadSucceeded(file, TEST_FILE);
			}
			else
			{

				ESP_LOGI(TAG, "== Sending to server failed");
				String response = http.getString();
				ESP_LOGI(TAG, "HTTP response code:");
				ESP_LOGI(TAG, "%d", httpResponseCode);
				ESP_LOGI(TAG, "HTTP response body:");
				ESP_LOGI(TAG, "%s", response);

				kalfy::files::uploadFailed(file);
			}
			http.end();
			ESP_LOGI(TAG, "== Upload complete");
		}
	}


}
