cmake_minimum_required(VERSION 3.25)
project(lilpet VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(sources.cmake)

add_library(lilpet_warnings INTERFACE)
target_compile_options(lilpet_warnings INTERFACE ${LILPET_WARNINGS})

if(EMSCRIPTEN)
    # ES module exposing the C API to JavaScript (browser and Node): lilpet.mjs + lilpet.wasm.
    # wasm/bindings.c exports the lpw_* functions (EMSCRIPTEN_KEEPALIVE); engine/wasm/engine.mjs wraps them.
    add_executable(lilpet_wasm ${LILPET_SOURCES} wasm/bindings.c)
    target_include_directories(lilpet_wasm PRIVATE include)
    target_link_libraries(lilpet_wasm PRIVATE lilpet_warnings)
    set_target_properties(lilpet_wasm PROPERTIES OUTPUT_NAME lilpet SUFFIX ".mjs")
    target_link_options(lilpet_wasm PRIVATE
        --no-entry
        -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createLilpet
        -sENVIRONMENT=web,node
        -sEXPORTED_FUNCTIONS=_lp_version
        -sEXPORTED_RUNTIME_METHODS=UTF8ToString,HEAPU8)
    return()
endif()

add_library(lilpet ${LILPET_SOURCES})
target_include_directories(lilpet PUBLIC include)
target_link_libraries(lilpet PRIVATE lilpet_warnings)

enable_testing()
add_executable(lilpet_tests tests/test_main.c tests/test_version.c tests/test_parity.c tests/test_eyes.c
    tests/test_render.c)
target_link_libraries(lilpet_tests PRIVATE lilpet lilpet_warnings m)
add_test(NAME lilpet_tests COMMAND lilpet_tests)

# Writes every reference scene as an image, to compare with prototype/reference.html (mise run engine:scenes)
add_executable(lilpet_render_scenes tools/render_scenes.c)
target_link_libraries(lilpet_render_scenes PRIVATE lilpet lilpet_warnings m)

# Renderer timings per reference scene and over an animation, in the host-release build (mise run engine:bench)
add_executable(lilpet_bench tools/bench.c)
target_compile_definitions(lilpet_bench PRIVATE _POSIX_C_SOURCE=200809L) # clock_gettime
target_link_libraries(lilpet_bench PRIVATE lilpet lilpet_warnings m)
