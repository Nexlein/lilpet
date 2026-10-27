# ESP-IDF application for the ESP32-S3 (target and options in sdkconfig.defaults).
# For now it benchmarks the renderer on the chip (main/main.c). Built by mise run firmware:build, locally and in CI,
# in the ESP-IDF image pinned in firmware/Dockerfile.
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(lilpet)
