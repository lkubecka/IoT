#define __USE_BLUETOOTH_
#ifdef __USE_BLUETOOTH_

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Arduino.h>
#include "record.hpp"
#include "time.hpp"
#include <sstream>
#include "esp_log.h"
#include "bluetooth.hpp"

using namespace kalfy;

constexpr  char  BLE::SERVICE_UUID[];
constexpr  char  BLE::CHARACTERISTIC_HAS_DATA_UUID[];
constexpr  char  BLE::CHARACTERISTIC_SEND_DATA_UUID[];
constexpr  char  BLE::CHARACTERISTIC_TIME_UUID[];
constexpr  int   BLE::MAX_BLE_PACKET_SIZE; 
constexpr  char  BLE::DESCRIPTOR_HAS_DATA[];
constexpr  char  BLE::DESCRIPTOR_SEND_DATA[];
constexpr  char  BLE::DESCRIPTOR_TIME[];

bool BLE::_BLEClientConnected = false;
bool BLE::_transferRequested = false;

BLE::BLE(const char* filename) : 
	_filename(filename),
	_secondsPassed(0),
	_hasDataCharacteristic(CHARACTERISTIC_HAS_DATA_UUID, BLECharacteristic::PROPERTY_READ), 
	_hasDataDescriptor(DESCRIPTOR_HAS_DATA), 
	_sendDataCharacteristic(CHARACTERISTIC_SEND_DATA_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_INDICATE),
	_sendDataDescriptor(DESCRIPTOR_SEND_DATA),
	_timeCharacteristic(CHARACTERISTIC_TIME_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ),
	_timeDescriptor(DESCRIPTOR_TIME)
{
}

void BLE::_init(void) {

	ESP_LOGI(TAG, "BLE Starting");
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
	ESP_LOGI(TAG, "BLE Started");
}

void BLE::_transferFile() {
	ESP_LOGI(TAG, "starting file transfer");
	File record = kalfy::record::openRecordForUpload(_filename.c_str());

	char sizeBuffer[MAX_BLE_PACKET_SIZE];
	snprintf(sizeBuffer, MAX_BLE_PACKET_SIZE, "%u", record.size());
	_sendDataCharacteristic.setValue(std::string(sizeBuffer));
	_sendDataCharacteristic.indicate();

	uint8_t buffer[20];
	size_t bytesRead = 1;

	while ((bytesRead = record.read(buffer, MAX_BLE_PACKET_SIZE)) > 0) {
		ESP_LOGI(TAG, "transferring %u bytes\n", bytesRead);
		_sendDataCharacteristic.setValue(buffer, bytesRead);
		_sendDataCharacteristic.indicate();
	}
	ESP_LOGI(TAG, "file transfer complete");
	kalfy::record::uploadSucceeded(record, _filename.c_str()); // deletes the underlying file
}

void BLE::run()
{
	_init();
	_hasDataCharacteristic.setValue(kalfy::record::hasData(_filename.c_str()) ? std::string("true") : std::string("false"));
	while (_secondsPassed < 60 || _transferRequested)
	{
		ESP_LOGI(TAG, "_secondsPassed: %d, _transferring: %d, _BLEClientConnected: %d\n", _secondsPassed, _transferRequested, _BLEClientConnected);
		if (_transferRequested) {
			_transferFile();
			return;
		}
		delay(1000);
		_secondsPassed++;
	}
	ESP_LOGI(TAG, "BLE koncim");
}

#endif