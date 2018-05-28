#ifndef BLE_H
#define BLE_H

#include "BLECharacteristic.h"
#include "BLEDescriptor.h"
#include "BLEServer.h"

namespace kalfy
{
	class BLE
	{
		public:
			static constexpr  char TAG[] = "BLE";

			static constexpr  char  SERVICE_UUID[] = "2e88c63b-bbf5-4312-a35c-8fc7f91c7d82";
			static constexpr  char  CHARACTERISTIC_HAS_DATA_UUID[] = "3686dd41-9af1-4b30-a464-3bee1cb4f13d";
			static constexpr  char  CHARACTERISTIC_SEND_DATA_UUID[] ="934c3fe1-01b4-4b43-9680-639aca577f23";
			static constexpr  char  CHARACTERISTIC_TIME_UUID[] = "31b1c4c1-49e5-4121-86bd-8841279a5bb6";
			static constexpr  int   MAX_BLE_PACKET_SIZE = 514; // 517 maximum allowed by BLE spec, but ESP32 allows only 514
			static constexpr  char  DESCRIPTOR_HAS_DATA[] = "516286d5-ebf2-4af8-9581-c33db072e55e";
			static constexpr  char  DESCRIPTOR_SEND_DATA[] = "eac2d00d-841a-4a68-a30d-fb81e3a72bb3";
			static constexpr  char  DESCRIPTOR_TIME[] = "516286d5-ebf2-4af8-9581-c33db072e55e";

			static bool _BLEClientConnected;
			static bool _transferRequested;

			BLE(const char* filename);
			void run();
		private:
			String _filename = "";
			int _secondsPassed = 0;

			void _transferFile();
			void _init(void);

			BLECharacteristic _hasDataCharacteristic;
			BLEDescriptor _hasDataDescriptor;
			BLECharacteristic _sendDataCharacteristic;
			BLEDescriptor _sendDataDescriptor;
			BLECharacteristic _timeCharacteristic;
			BLEDescriptor _timeDescriptor;

		class MyServerCallbacks : public BLEServerCallbacks {
			void onConnect(BLEServer* pServer) {
				kalfy::BLE::_BLEClientConnected = true;
			};

			void onDisconnect(BLEServer* pServer) {
				kalfy::BLE::_BLEClientConnected = false;
			}
		};

		class SendDataCharacteristicCallbacks : public BLECharacteristicCallbacks {
			void onWrite(BLECharacteristic* sendDataCharacteristic) {
				ESP_LOGI(TAG, "onWrite to sendData characteristic");
				std::string value = sendDataCharacteristic->getValue();
				if (value.compare(std::string("true")) == 0) {
					kalfy::BLE::_transferRequested = true;
					ESP_LOGI(TAG, "transfer requested");
				}
			}
		};

		class TimeCharacteristicCallbacks : public BLECharacteristicCallbacks {
			void onWrite(BLECharacteristic* timeCharacteristic) {
				ESP_LOGI(TAG, "setting time via BLE");
				std::string value = timeCharacteristic->getValue();
				long time = atol(value.c_str());
				kalfy::time::setTime(time);
				ESP_LOGI(TAG, "time set");
			}
		};
	};
}

#endif