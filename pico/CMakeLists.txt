cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD pico_w CACHE STRING "Board type")

# --- PORTABILITY FIX START ---
# This ignores the global computer settings and uses the SDK inside your folder
set(PICO_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/pico-sdk")
include(pico_sdk_import.cmake)
# --- PORTABILITY FIX END ---

project(stacking C CXX ASM)

pico_sdk_init()

add_executable(stacking 
    stacking.c
    usb_descriptors.c
)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(stacking 0)
pico_enable_stdio_usb(stacking 1) # Set to 1 to see your log_printf over USB!

# Add the standard library to the build
target_link_libraries(stacking
    pico_stdlib
    pico_cyw43_arch_lwip_poll
    pico_multicore
    hardware_sync
    hardware_gpio
    hardware_timer
    pico_unique_id
    tinyusb_device
    tinyusb_board
)

target_include_directories(stacking PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

# These relative paths work now because PICO_SDK_PATH is set to your local folder
target_include_directories(stacking PRIVATE
    ${PICO_SDK_PATH}/lib/lwip/src/include
)

pico_add_extra_outputs(stacking)