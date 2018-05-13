#ifndef BLE_H
#define BLE_H

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

namespace kalfy
{
	class ble
	{
		public:
			ble();
			void run();
		private:
			BLECharacteristic _hasDataCharacteristic;
			BLEDescriptor _hasDataDescriptor;
			BLECharacteristic _sendDataCharacteristic;
			BLEDescriptor _sendDataDescriptor;
			BLECharacteristic _timeCharacteristic;
			BLEDescriptor _timeDescriptor;
			int _secondsPassed;
			bool _BLEClientConnected;
			bool _transferRequested;


			void _transferFile(void);
			void _init(void);
	}
}

#endif