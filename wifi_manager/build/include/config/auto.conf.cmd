deps_config := \
	/home/tony/esp/esp-idf-v2.1/components/aws_iot/Kconfig \
	/home/tony/esp/esp-idf-v2.1/components/bt/Kconfig \
	/home/tony/esp/esp-idf-v2.1/components/esp32/Kconfig \
	/home/tony/esp/esp-idf-v2.1/components/ethernet/Kconfig \
	/home/tony/esp/esp-idf-v2.1/components/fatfs/Kconfig \
	/home/tony/esp/esp-idf-v2.1/components/freertos/Kconfig \
	/home/tony/esp/esp-idf-v2.1/components/log/Kconfig \
	/home/tony/esp/esp-idf-v2.1/components/lwip/Kconfig \
	/home/tony/esp/esp-idf-v2.1/components/mbedtls/Kconfig \
	/home/tony/esp/esp-idf-v2.1/components/openssl/Kconfig \
	/home/tony/esp/esp-idf-v2.1/components/spi_flash/Kconfig \
	/home/tony/esp/esp-idf-v2.1/components/bootloader/Kconfig.projbuild \
	/home/tony/esp/esp-idf-v2.1/components/esptool_py/Kconfig.projbuild \
	/home/tony/esp/esp-idf-v2.1/components/partition_table/Kconfig.projbuild \
	/home/tony/esp/esp-idf-v2.1/Kconfig

include/config/auto.conf: \
	$(deps_config)


$(deps_config): ;
