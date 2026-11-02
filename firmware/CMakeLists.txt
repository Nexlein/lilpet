# ESP-IDF application for the ESP32-S3 (target and options in sdkconfig.defaults).
# It shows the eyes, takes looks from the PC app and times the renderer (main/main.c). Built by mise run firmware:build,
# locally and in CI, in the ESP-IDF image pinned in firmware/Dockerfile.
cmake_minimum_required(VERSION 3.16)
set(PROJECT_VER "0.4.0") # firmware version, reported to the PC app
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(lilpet)
