cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

project(bus6502_project)

pico_sdk_init()

add_executable(bus6502
  bus6502.c
)

pico_generate_pio_header(bus6502 ${CMAKE_CURRENT_LIST_DIR}/bus6502.pio)

pico_enable_stdio_usb(bus6502 1)

pico_add_extra_outputs(bus6502)

target_link_libraries(bus6502 pico_stdlib hardware_pio)
