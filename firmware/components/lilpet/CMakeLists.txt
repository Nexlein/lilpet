# The engine as an ESP-IDF component: same sources and warnings as the host build (engine/sources.cmake)
set(engine_dir ${CMAKE_CURRENT_LIST_DIR}/../../../engine)
include(${engine_dir}/sources.cmake)
list(TRANSFORM LILPET_SOURCES PREPEND ${engine_dir}/)

idf_component_register(SRCS ${LILPET_SOURCES} INCLUDE_DIRS ${engine_dir}/include)
target_compile_options(${COMPONENT_LIB} PRIVATE ${LILPET_WARNINGS})
