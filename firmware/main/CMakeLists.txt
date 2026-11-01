idf_component_register(SRCS main.c display.c commands.c look_store.c
                       REQUIRES lilpet esp_timer esp_lcd esp_driver_spi esp_driver_gpio esp_driver_usb_serial_jtag
                                esp_app_format nvs_flash)
