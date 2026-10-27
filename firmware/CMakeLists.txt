# ESP-IDF application for the ESP32-S3 (target set in sdkconfig.defaults).
# For now it only proves the engine builds for the chip; CI builds it with espressif/esp-idf-ci-action.
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(lilpet)
