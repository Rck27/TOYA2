# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.11)

add_definitions(
-DARDUINO_USB_MODE
-DARDUINO_USB_CDC_ON_BOOT
)

# spiffs_create_partition_image(storage spiffs_image FLASH_IN_PROJECT)


include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(toya)
