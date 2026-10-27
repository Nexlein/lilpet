# Engine sources and warning flags, shared by engine/CMakeLists.txt (host, WebAssembly) and the ESP-IDF component
# (firmware/components/lilpet). Paths are relative to engine/.
set(LILPET_SOURCES src/version.c src/rng.c src/emotions.c src/eyes.c src/pose.c)

# Strict warnings, as errors. -Wdouble-promotion catches implicit doubles, which the ESP32-S3 emulates in software.
set(LILPET_WARNINGS
    -Wall -Wextra -Wpedantic -Werror
    -Wconversion -Wsign-conversion -Wdouble-promotion -Wshadow
    -Wstrict-prototypes -Wmissing-prototypes)
