cmake_minimum_required(VERSION 3.13)

set(PICO_SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib/pico-sdk)
include(pico_sdk_import.cmake)

project(rp2040_rubber_ducky)
pico_sdk_init()

add_executable(rp2040_rubber_ducky
    src/main.c
    src/sd_card.c
    src/diskio.c
    lib/fatfs/source/ff.c
    lib/fatfs/source/ffsystem.c
    lib/fatfs/source/ffunicode.c
)

target_include_directories(rp2040_rubber_ducky PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/fatfs/source
)

target_link_libraries(rp2040_rubber_ducky
    pico_stdlib
    tinyusb_device
    tinyusb_board
    hardware_spi
    hardware_gpio
    hardware_timer
)

pico_enable_stdio_usb(rp2040_rubber_ducky 1)
pico_enable_stdio_uart(rp2040_rubber_ducky 0)
pico_add_extra_outputs(rp2040_rubber_ducky)
