#include "File.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "string.h"

static const char *TAG = "File";

File::File(const char * fileName) {
	_file = NULL;
	_fileName = std::string(fileName);
}



void File::uploadSucceeded()
{
	deleteFile();
}

void File::uploadFailed()
{
	fclose(_file);
}

void File::appendToFile( const char * line)
{
	esp_log_write(ESP_LOG_DEBUG, TAG,"== Appending to file\n");
	
	ESP_LOGI(TAG, "Opening file for writing");
	_file = fopen(_fileName.c_str(), "a");
	if (_file == NULL) {
		ESP_LOGE(TAG, "Failed to open file for writing: %s", _fileName.c_str());
		return;
	}
	
	size_t size = fprintf(_file, "%s\n", line);
	esp_log_write(ESP_LOG_DEBUG, TAG,"%s", size ? "== Appending succeeded" : "== Appending failed - could not print to file");
	fflush(_file);
	fclose(_file);
}


void File::deleteFile()
{
	esp_log_write(ESP_LOG_DEBUG, TAG, "== Deleting file\n");
	fclose(_file);
	unlink(_fileName.c_str());
}


void File::printFile(void)
{
	esp_log_write(ESP_LOG_DEBUG, TAG,"== Printing file\n");
	
	ESP_LOGI(TAG, "Opening file for reading");
	_file = fopen(_fileName.c_str(), "r");
	if (_file == NULL) {
		ESP_LOGE(TAG, "Failed to open file for reading: %s", _fileName.c_str());
		return;
	}

	char line[MAX_LINE_LENGTH];
	int i=0;
	while (fgets(line, MAX_LINE_LENGTH, _file)) {
		line[strlen(line) - 1] = '\0';
        printf("%d: %s\n", i++, line);
	}    
 
	fclose(_file);
}