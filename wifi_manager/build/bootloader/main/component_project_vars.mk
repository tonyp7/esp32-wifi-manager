# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(IDF_PATH)/components/bootloader/src/main/include
COMPONENT_LDFLAGS += -L $(IDF_PATH)/components/bootloader/src/main -lmain -T esp32.bootloader.ld -T $(IDF_PATH)/components/esp32/ld/esp32.rom.ld -T $(IDF_PATH)/components/esp32/ld/esp32.peripherals.ld -T esp32.bootloader.rom.ld
COMPONENT_LINKER_DEPS += $(IDF_PATH)/components/bootloader/src/main/esp32.bootloader.ld $(IDF_PATH)/components/esp32/ld/esp32.rom.ld $(IDF_PATH)/components/esp32/ld/esp32.peripherals.ld $(IDF_PATH)/components/bootloader/src/main/esp32.bootloader.rom.ld
COMPONENT_SUBMODULES += 
main-build: 
