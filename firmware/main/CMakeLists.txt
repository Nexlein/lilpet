# One image per board: each header (display.h, imu.h) is an interface, and the board's parts are linked here. Its pins
# and mounting come from boards/<board>/board_config.h. A display with no TE signal links display_paced.c as well.
set(srcs main.c board.c commands.c look_store.c)
if(CONFIG_LILPET_BOARD_ATOMS3R)
    set(board atoms3r)
    list(APPEND srcs display_atoms3r.c display_paced.c imu_bmi270.c bmi270_config.c)
elseif(CONFIG_LILPET_BOARD_WAVESHARE_132)
    set(board waveshare_132)
    list(APPEND srcs display_co5300.c imu_none.c)
elseif(CONFIG_LILPET_BOARD_STAMP_S3A)
    set(board stamp_s3a)
    list(APPEND srcs display_co5300.c display_paced.c imu_none.c)
elseif(NOT CMAKE_BUILD_EARLY_EXPANSION) # no sdkconfig yet while ESP-IDF scans the components
    message(FATAL_ERROR "No board chosen: the wrong image would drive the wrong pins. Build with "
                        "BOARD=<board> mise run firmware:build (atoms3r, waveshare_132 or stamp_s3a).")
endif()

idf_component_register(SRCS ${srcs} INCLUDE_DIRS ../boards/${board}
                       REQUIRES lilpet esp_timer esp_lcd esp_driver_spi esp_driver_gpio esp_driver_i2c
                                esp_driver_usb_serial_jtag esp_app_format nvs_flash)
