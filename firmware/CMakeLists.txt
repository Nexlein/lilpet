# ESP-IDF application for the ESP32-S3 (target and options in sdkconfig.defaults).
# It shows the eyes, takes commands from the PC app and times the renderer (main/main.c). One image per board
# (boards/<board>): built by BOARD=<board> mise run firmware:build, locally and in CI, in the ESP-IDF image pinned in
# firmware/Dockerfile.
cmake_minimum_required(VERSION 3.16)
set(PROJECT_VER "0.5.0") # firmware version, reported to the PC app

# A board is a folder of boards/ with a board.cmake. Checked here, before ESP-IDF first scans the components: that scan
# runs as a separate script, which does not see LILPET_BOARD.
file(GLOB boards RELATIVE ${CMAKE_CURRENT_LIST_DIR}/boards ${CMAKE_CURRENT_LIST_DIR}/boards/*/board.cmake)
list(TRANSFORM boards REPLACE "/board.cmake$" "")
if(NOT LILPET_BOARD IN_LIST boards)
    list(JOIN boards ", " boards)
    message(FATAL_ERROR "No such board: \"${LILPET_BOARD}\". The wrong image would drive the wrong pins. Build with "
                        "BOARD=<board> mise run firmware:build (${boards}).")
endif()

# ESP-IDF options: the common ones, then those of a board that needs its own
set(SDKCONFIG_DEFAULTS sdkconfig.defaults)
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/boards/${LILPET_BOARD}/sdkconfig.defaults)
    list(APPEND SDKCONFIG_DEFAULTS boards/${LILPET_BOARD}/sdkconfig.defaults)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(lilpet)
