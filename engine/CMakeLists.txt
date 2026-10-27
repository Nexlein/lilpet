cmake_minimum_required(VERSION 3.25)
project(lilpet VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(sources.cmake)

add_library(lilpet_warnings INTERFACE)
target_compile_options(lilpet_warnings INTERFACE ${LILPET_WARNINGS})

if(EMSCRIPTEN)
    # ES module exposing the C API to JavaScript (browser and Node): lilpet.mjs + lilpet.wasm
    add_executable(lilpet_wasm ${LILPET_SOURCES})
    target_include_directories(lilpet_wasm PRIVATE include)
    target_link_libraries(lilpet_wasm PRIVATE lilpet_warnings)
    set_target_properties(lilpet_wasm PROPERTIES OUTPUT_NAME lilpet SUFFIX ".mjs")
    target_link_options(lilpet_wasm PRIVATE
        --no-entry
        -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createLilpet
        -sENVIRONMENT=web,node
        -sEXPORTED_FUNCTIONS=_lp_version
        -sEXPORTED_RUNTIME_METHODS=UTF8ToString)
    return()
endif()

add_library(lilpet ${LILPET_SOURCES})
target_include_directories(lilpet PUBLIC include)
target_link_libraries(lilpet PRIVATE lilpet_warnings)

enable_testing()
add_executable(lilpet_tests tests/test_main.c tests/test_version.c)
target_link_libraries(lilpet_tests PRIVATE lilpet lilpet_warnings m)
add_test(NAME lilpet_tests COMMAND lilpet_tests)
