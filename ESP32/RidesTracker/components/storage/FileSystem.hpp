#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#include <string>
#include "esp_spiffs.h"

class FileSystem
{
	private:
		static const int _maxFiles = 5;
		static const int _formatIfFail = true;
		std::string _mountPoint;
	public:
		FileSystem() {_mountPoint = std::string("/spiffs");};
		void begin(void);
		void info(void);
		void end(void);
};

#endif