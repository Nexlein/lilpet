# One image per board: each header (display.h, imu.h) is an interface, and boards/<board>/board.cmake links the board's
# parts. Its pins and mounting come from board_config.h, in the same folder.
set(srcs main.c board.c commands.c look_store.c display_paced.c) # display_paced.c: without a TE signal
if(NOT CMAKE_BUILD_EARLY_EXPANSION) # ESP-IDF's first scan only reads REQUIRES, and has no LILPET_BOARD
    include(../boards/${LILPET_BOARD}/board.cmake)
endif()

idf_component_register(SRCS ${srcs} INCLUDE_DIRS ../boards/${LILPET_BOARD}
                       REQUIRES lilpet esp_timer esp_lcd esp_driver_spi esp_driver_gpio esp_driver_i2c
                                esp_driver_usb_serial_jtag esp_app_format efuse nvs_flash esp_adc spi_flash)
# A board's name is its folder: a copied board cannot keep the name of the first one
target_compile_definitions(${COMPONENT_LIB} PRIVATE BOARD_NAME="${LILPET_BOARD}")
# The engine's warnings (engine/sources.cmake) on top of ESP-IDF's, but -Wpedantic, which ESP-IDF's headers trip, and
# -Wmissing-prototypes, since app_main has none: a double literal or a narrowing conversion fails the build here too
target_compile_options(${COMPONENT_LIB} PRIVATE -Wconversion -Wsign-conversion -Wdouble-promotion -Wshadow
                                                -Wstrict-prototypes)
