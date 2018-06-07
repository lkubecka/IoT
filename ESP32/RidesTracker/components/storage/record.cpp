#include <Arduino.h>
#include <SPIFFS.h>
#include <FS.h>
#include <HTTPClient.h>
#include <sys/time.h>
#include "wifi.hpp"
#include "record.hpp"
#include "files.hpp"
#include "time.hpp"
#include "WiFiClientSecure.h"

#ifdef ARDUINO_ARCH_ESP32
#include "esp32-hal-log.h"
#endif

const int ODOCYCLE_PORT = 443;
const char* ODOCYCLE_SERVER = "ridestracker.azurewebsites.net";
const char* ODOCYCLE_URL = "/api/v1/Records/";
const char* ODOCYCLE_ID = "c0f6928d-cac5-4025-adc4-bad65f06b1d8";
const char* ODOCYCLE_TOKEN = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiI5YmQwYzhmMC05ZTI0LTQxZTgtYjkwNi02ZDI3MGFjNGFkN2UiLCJqdGkiOiI3ZmM2ODNmOC04ZWEwLTQzYjMtODM5YS1mYjkwMmQ1M2FmYmUiLCJleHAiOjE1MzA2OTQ4MjIsImlzcyI6ImF6dXJlLWRldiIsImF1ZCI6ImF6dXJlLWRldiJ9.Nf1HgLzT9ZEq7cp-T99KMA9xlpFCIxU-K2J0HYXHAVE";

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
		const char* TEST_FILE = "/test.txt";

		void saveRevolution(timeval &now,  const char * filename)
		{
			char buffer[32];
			snprintf(buffer, sizeof(buffer), "t%ld|%ld", now.tv_sec, now.tv_usec);
			Serial.println(buffer);
			kalfy::files::appendToFile(filename, buffer);
		}

		void savePressure(int32_t pressurePa, const char * filename)
		{
			char buffer[16];
			snprintf(buffer, sizeof(buffer), "p%d", pressurePa);
			Serial.println(buffer);
			kalfy::files::appendToFile(filename, buffer);
		}

		bool hasData(const char * filename) {
			return kalfy::files::hasData(filename);
		}

		File openRecordForUpload(const char * filename)
		{
			return kalfy::files::openFileForUpload(filename);
		}

		void uploadSucceeded(File file, const char * filename)
		{
			kalfy::files::uploadSucceeded(file, filename);
		}

		void uploadFailed(File file)
		{
			kalfy::files::uploadFailed(file);
		}

		void printAll(const char * filename)
		{

			ESP_LOGI(TAG,"=== printAll called");

			File file = kalfy::files::openFileForUpload(filename);
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
		void clear(const char * filename) {
			kalfy::files::deleteFile(filename);
		}

		void createTestFile(const char * filename) {
            printAll(filename);
			kalfy::files::deleteFile(filename);

			const int32_t presure = 101250; 
			struct timeval now = kalfy::time::getCurrentTime();
			
			saveRevolution(now, filename);
			savePressure(presure, filename);
			now = kalfy::time::getCurrentTime();
			saveRevolution(now, filename);
			savePressure(presure, filename);

		}

		
		void uploadFile(const char * apiUrl, const char * deviceId, const char * token, const char * ca_cert, const char * filename)
		{
			
			ESP_LOGI(TAG, "=== uploadAll called");
			if (apiUrl == nullptr || deviceId == nullptr)
			{
				ESP_LOGI(TAG,"Some of the arguments is null!");
				return;
			}

			File file = kalfy::files::openFileForUpload(filename);
			if (!file || file.size() == 0)
			{
				// the HTTPClient API has some issue with zero-size files, crashes, so we must check for an empty file
				ESP_LOGI(TAG, "=== Nothing to send");
				return;
			}

			HTTPClient http;
			http.begin(String(apiUrl) + String(deviceId), ca_cert);
			http.addHeader("Content-Type", "multipart/form-data; boundary=5ed41908-0515-4f7f-ae1a-d3b2ebee72ea");
			//http.addHeader("Content-Type", "multipart/form-data; boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW");
			http.addHeader("Authorization", "Bearer " + String(token));
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

		void uploadMultipartFile(const char * apiUrl, const char * deviceId, const char * token, const char * ca_cert, const char * filename)
		{
			
			ESP_LOGI(TAG, "=== uploadAll called");
			if (apiUrl == nullptr || deviceId == nullptr)
			{
				ESP_LOGI(TAG,"Some of the arguments is null!");
				return;
			}

			File file = kalfy::files::openFileForUpload(filename);
			if (!file || file.size() == 0)
			{
				// the HTTPClient API has some issue with zero-size files, crashes, so we must check for an empty file
				ESP_LOGI(TAG, "=== Nothing to send");
				return;
			}

			WiFiClientSecure client;

			String fileName = file.name();
			String fileSize = String(file.size());

			Serial.println();
			Serial.println("file exists");
			Serial.println(file);

			if (file) {

				// print content length and host
				Serial.println("contentLength");
				Serial.println(fileSize);
				Serial.print("connecting to ");
				Serial.println(ODOCYCLE_SERVER);

				// try connect or return on fail
				client.setCACert(ODOCYCLE_CERT);
				if (!client.connect(ODOCYCLE_SERVER, ODOCYCLE_PORT, ODOCYCLE_CERT, NULL, NULL)) {
					Serial.println("https POST connection failed");
					//return String("Post Failure");
					return;
				}

				// We now create a URI for the request
				String url = String("https://")+String(ODOCYCLE_SERVER) + String(ODOCYCLE_URL) + String(ODOCYCLE_ID);
				Serial.println("Connected to server");
				Serial.print("Requesting URL: ");
				Serial.println(url.c_str());


				// Make a HTTP request and add HTTP headers
				String boundary = "SolarServerBoundaryjg2qVIUS8teOAbN3";
				String contentType = "form-data"; //"text/plain";
				String portString = String(ODOCYCLE_PORT);
				String hostString = String("https://") + String(ODOCYCLE_SERVER);
				

				// post header
				String postHeader = "POST " + url + " HTTP/1.1\r\n";
				postHeader += "Authorization: Bearer " + String(ODOCYCLE_TOKEN) + "\r\n";
				postHeader += "Host: " + hostString + ":" + portString + "\r\n";
				postHeader += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
				
				// key header
				String keyHeader = "--" + boundary + "\r\n";
				keyHeader += "Content-Disposition: form-data; name=\"record\"\r\n\r\n";
				keyHeader += "${filename}\r\n";

				// request header
				String requestHead = "--" + boundary + "\r\n";
				requestHead += "Content-Disposition: form-data; name=\"file\"; filename=\"" + fileName + "\"\r\n";
				requestHead += "Content-Type: " + contentType + "\r\n\r\n";

				// request tail
				String tail = "\r\n--" + boundary + "--\r\n\r\n";

				// content length
				int contentLength = keyHeader.length() + requestHead.length() + file.size() + tail.length();
				postHeader += "Content-Length: " + String(contentLength, DEC) + "\n\n";

				// send post header
				char charBuf0[postHeader.length() + 1];
				postHeader.toCharArray(charBuf0, postHeader.length() + 1);
				client.write(charBuf0);
				Serial.print(charBuf0);

				// send key header
				char charBufKey[keyHeader.length() + 1];
				keyHeader.toCharArray(charBufKey, keyHeader.length() + 1);
				client.write(charBufKey);
				Serial.print(charBufKey);

				// send request buffer
				char charBuf1[requestHead.length() + 1];
				requestHead.toCharArray(charBuf1, requestHead.length() + 1);
				client.write(charBuf1);
				Serial.print(charBuf1);

				// create buffer
				const int bufSize = 2048;
				byte clientBuf[bufSize];
				int clientCount = 0;

				while (file.available()) {

					clientBuf[clientCount] = file.read();

					clientCount++;

					if (clientCount > (bufSize - 1)) {
						client.write((const uint8_t *)clientBuf, bufSize);
						clientCount = 0;
					}

				}

				if (clientCount > 0) {
					client.write((const uint8_t *)clientBuf, clientCount);
					Serial.println("Sent LAST buffer");
				}

				// send tail
				char charBuf3[tail.length() + 1];
				tail.toCharArray(charBuf3, tail.length() + 1);
				client.write(charBuf3);
				Serial.print(charBuf3);

				// Read all the lines of the reply from server and print them to Serial
				Serial.println("request sent");
				String responseHeaders = "";

				while (client.connected()) {
					// Serial.println("while client connected");
					String line = client.readStringUntil('\n');
					Serial.println(line);
					responseHeaders += line;
					if (line == "\r") {
						Serial.println("headers received");
						break;
					}
				}
				
				String line = client.readStringUntil('\n');

				Serial.println("reply was:");
				Serial.println("==========");
				Serial.println(line);
				Serial.println("==========");
				Serial.println("closing connection");

				// close the file:
				file.close();
				//return responseHeaders;	
			}
		}

	}
}
