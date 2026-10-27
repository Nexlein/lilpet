# ESP-IDF application for the ESP32-S3 (target and options in sdkconfig.defaults).
# For now it benchmarks the renderer on the chip (main/main.c). Built locally in the official ESP-IDF Docker image
# (mise run firmware:build) and in CI with espressif/esp-idf-ci-action.
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(lilpet)
