idf_component_register(SRCS main.c board.c display.c display_co5300.c display_atoms3r.c imu_bmi270.c bmi270_config.c
                            commands.c look_store.c
                       REQUIRES lilpet esp_timer esp_lcd esp_driver_spi esp_driver_gpio esp_driver_i2c
                                esp_driver_usb_serial_jtag esp_app_format efuse nvs_flash)
