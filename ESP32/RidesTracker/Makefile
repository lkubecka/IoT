#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#
#V := 1

PROJECT_NAME := RidesTracker

EXTRA_COMPONENT_DIRS := $(CURDIR)/components

EXTRA_COMPONENTS := i2cdev bmp180 revolutions_counter utils preferences storage

DEFAULT_COMPONENTS := \
   esptool_py \
	heap \
	soc \
	esp32 \
	driver \
	pthread \
	partition_table \
	log \
	bootloader_support \
	micro-ecc \
	mbedtls \
	lwip \
	tcpip_adapter \
	ethernet \
	app_update \
	app_trace \
	xtensa-debug-module \
	nvs_flash \
	wpa_supplicant \
	main \
	newlib \
	vfs \
	spi_flash \
	console \
	freertos \
	ulp \
	spiffs


COMPONENTS := $(DEFAULT_COMPONENTS) $(EXTRA_COMPONENTS)

include $(IDF_PATH)/make/project.mk
