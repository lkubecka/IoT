#include "FileSystem.hpp"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "FileSystem";

void FileSystem::begin(void)
{
	ESP_LOGI(TAG, "Initializing SPIFFS");

	if(esp_spiffs_mounted(NULL)){
		ESP_LOGW(TAG, "SPIFFS Already Mounted!");
	} else {
		esp_vfs_spiffs_conf_t conf;
		
		conf.base_path = _mountPoint.c_str(),
		conf.partition_label = NULL,
		conf.max_files = _maxFiles,
		conf.format_if_mount_failed = _formatIfFail;
			
		esp_err_t ret = esp_vfs_spiffs_register(&conf);

		if (ret != ESP_OK) {
			if (ret == ESP_FAIL) {
				ESP_LOGE(TAG, "Failed to mount or format filesystem");
			} else if (ret == ESP_ERR_NOT_FOUND) {
				ESP_LOGE(TAG, "Failed to find SPIFFS partition");
			} else {
				ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
			}
		}	
		ESP_LOGW(TAG, "SPIFFS mounted.");
	}
}

void FileSystem::info(void) {
	size_t total = 0, used = 0;

	esp_err_t ret = esp_spiffs_info(NULL, &total, &used);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
	} else {
		ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
	}
}

void FileSystem::end(void) {
	esp_vfs_spiffs_unregister(NULL);
    ESP_LOGI(TAG, "SPIFFS unmounted");
}
