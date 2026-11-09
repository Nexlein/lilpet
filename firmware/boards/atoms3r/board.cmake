# Parts of the board, files of firmware/main (third-party ones in firmware/third_party). A display with no TE signal
# links display_paced.c as well.
list(APPEND srcs display_atoms3r.c display_paced.c imu_bmi270.c ../third_party/bmi270/bmi270_config.c)
