#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Arduino.h>
#include "record.hpp"
#include "time.hpp"
#include <sstream>

namespace kalfy
{
	namespace ble
	{
#define SERVICE_UUID "2e88c63b-bbf5-4312-a35c-8fc7f91c7d82"
#define CHARACTERISTIC_HAS_DATA_UUID "3686dd41-9af1-4b30-a464-3bee1cb4f13d"
#define CHARACTERISTIC_SEND_DATA_UUID "934c3fe1-01b4-4b43-9680-639aca577f23"
#define CHARACTERISTIC_TIME_UUID "31b1c4c1-49e5-4121-86bd-8841279a5bb6"
#define MAX_BLE_PACKET_SIZE 514 // 517 maximum allowed by BLE spec, but ESP32 allows only 514

		BLECharacteristic _hasDataCharacteristic(CHARACTERISTIC_HAS_DATA_UUID, BLECharacteristic::PROPERTY_READ);
		BLEDescriptor _hasDataDescriptor("516286d5-ebf2-4af8-9581-c33db072e55e");
		BLECharacteristic _sendDataCharacteristic(CHARACTERISTIC_SEND_DATA_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_INDICATE);
		BLEDescriptor _sendDataDescriptor("eac2d00d-841a-4a68-a30d-fb81e3a72bb3");
		BLECharacteristic _timeCharacteristic(CHARACTERISTIC_TIME_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ);
		BLEDescriptor _timeDescriptor("fba1d66e-ae5e-4277-add7-3b0fe0faf0bf");

		int _secondsPassed = 0;
		bool _BLEClientConnected = false;
		bool _transferRequested = false;

		class MyServerCallbacks : public BLEServerCallbacks {
			void onConnect(BLEServer* pServer) {
				_BLEClientConnected = true;
			};

			void onDisconnect(BLEServer* pServer) {
				_BLEClientConnected = false;
			}
		};

		class SendDataCharacteristicCallbacks : public BLECharacteristicCallbacks {
			void onWrite(BLECharacteristic* sendDataCharacteristic) {
				Serial.println("onWrite to sendData characteristic");
				std::string value = sendDataCharacteristic->getValue();
				if (value.compare(std::string("true")) == 0) {
					_transferRequested = true;
					Serial.println("transfer requested");
				}
			}
		};

		class TimeCharacteristicCallbacks : public BLECharacteristicCallbacks {
			void onWrite(BLECharacteristic* timeCharacteristic) {
				Serial.println("setting time via BLE");
				std::string value = timeCharacteristic->getValue();
				long time = atol(value.c_str());
				kalfy::time::setTime(time);
				Serial.println("time set");
			}
		};

		void _init()
		{
			_secondsPassed = 0;
			_BLEClientConnected = false;
			_transferRequested = false;
			Serial.println("BLE Starting");
			BLEDevice::init("RidesTrack device");
			BLEDevice::setMTU(MAX_BLE_PACKET_SIZE + 3); // 517 maximum allowed by BLE spec, but ESP32 allows only 514 (3 bytes seem to be reserved): https://github.com/espressif/esp-idf/blob/a4fe12cb6d195a580a57690ceed45e1e06dc58e0/components/bt/bluedroid/stack/gatt/att_protocol.c#L324
			// Create the BLE Server
			BLEServer *pServer = BLEDevice::createServer();
			pServer->setCallbacks(new MyServerCallbacks());

			// Create the BLE Service
			BLEService *service = pServer->createService(SERVICE_UUID);

			// Add hasData Characteristic
			service->addCharacteristic(&_hasDataCharacteristic);
			_hasDataDescriptor.setValue(std::string("Has data (true/false)?"));
			_hasDataCharacteristic.addDescriptor(&_hasDataDescriptor);
			_hasDataCharacteristic.setValue(std::string("false"));

			// Add sendData Characteristic
			service->addCharacteristic(&_sendDataCharacteristic);
			_sendDataDescriptor.setValue(std::string("Send data"));
			_sendDataCharacteristic.addDescriptor(&_sendDataDescriptor);
			_sendDataCharacteristic.addDescriptor(new BLE2902());
			_sendDataCharacteristic.setValue(std::string("false"));
			_sendDataCharacteristic.setCallbacks(new SendDataCharacteristicCallbacks());

			// Add time Characteristic
			service->addCharacteristic(&_timeCharacteristic);
			_timeDescriptor.setValue(std::string("Set time"));
			std::string time;
			std::stringstream strstream;
			strstream << kalfy::time::getCurrentTime().tv_sec;
			strstream >> time;
			_timeCharacteristic.setValue(time);
			_timeCharacteristic.addDescriptor(&_timeDescriptor);
			_timeCharacteristic.setCallbacks(new TimeCharacteristicCallbacks());

			pServer->getAdvertising()->addServiceUUID(SERVICE_UUID);

			service->start();
			// Start advertising
			pServer->getAdvertising()->start();
			Serial.println("BLE Started");
		}

		void _transferFile() {
			Serial.println("starting file transfer");
			File record = kalfy::record::openRecordForUpload();

			char sizeBuffer[MAX_BLE_PACKET_SIZE];
			snprintf(sizeBuffer, MAX_BLE_PACKET_SIZE, "%u", record.size());
			_sendDataCharacteristic.setValue(std::string(sizeBuffer));
			_sendDataCharacteristic.indicate();

			uint8_t buffer[20];
			size_t bytesRead = 1;

			while ((bytesRead = record.read(buffer, MAX_BLE_PACKET_SIZE)) > 0) {
				Serial.printf("transferring %u bytes\n", bytesRead);
				_sendDataCharacteristic.setValue(buffer, bytesRead);
				_sendDataCharacteristic.indicate();
			}
			Serial.println("file transfer complete");
			kalfy::record::uploadSucceeded(record); // deletes the underlying file
		}

		void run()
		{
			_init();
			_hasDataCharacteristic.setValue(kalfy::record::hasData() ? std::string("true") : std::string("false"));
			while (_secondsPassed < 60 || _transferRequested)
			{
				Serial.printf("_secondsPassed: %d, _transferring: %d, _BLEClientConnected: %d\n", _secondsPassed, _transferRequested, _BLEClientConnected);
				if (_transferRequested) {
					_transferFile();
					return;
				}
				delay(1000);
				_secondsPassed++;
			}
			Serial.println("BLE koncim");
		}
	}
}