cmake_minimum_required(VERSION 3.20)
set(PICO_BOARD pico2_w CACHE STRING "Target board")
if(NOT PICO_BOARD STREQUAL "pico2_w")
    message(FATAL_ERROR "This firmware targets Pico 2 W only")
endif()
set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/resources/pico-sdk" CACHE PATH "Pico SDK")
set(BLUEPAD32_ROOT "${CMAKE_CURRENT_LIST_DIR}/resources/bluepad32" CACHE PATH "Bluepad32")
set(PICO_BTSTACK_PATH "${BLUEPAD32_ROOT}/external/btstack" CACHE PATH "BTstack")
set(BTSTACK_ROOT "${PICO_BTSTACK_PATH}")
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(pico2w_receiver C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(RECEIVER_USB_MODE "xinput" CACHE STRING "USB output: xinput or switch")
set_property(CACHE RECEIVER_USB_MODE PROPERTY STRINGS xinput switch)
if(RECEIVER_USB_MODE STREQUAL "switch")
    set(receiver_usb_sources src/usb_switch.c src/usb_switch_descriptors.c src/switch_report.c)
elseif(RECEIVER_USB_MODE STREQUAL "xinput")
    set(receiver_usb_sources src/usb_xinput.c src/usb_descriptors.c)
else()
    message(FATAL_ERROR "RECEIVER_USB_MODE must be xinput or switch")
endif()
# Config headers must also be visible when compiling Bluepad32 and SDK libraries.
include_directories(${CMAKE_CURRENT_LIST_DIR}/src)
pico_sdk_init()
# BTstack renamed BLE HIDS client to HIDS host after SDK 2.2.0. Adapt SDK's
# source list without editing either dependency; Bluepad32 uses the new API.
get_target_property(ble_sources pico_btstack_ble INTERFACE_SOURCES)
string(REPLACE "/hids_client.c" "/hids_host.c" ble_sources "${ble_sources}")
set_property(TARGET pico_btstack_ble PROPERTY INTERFACE_SOURCES "${ble_sources}")
add_subdirectory(${BLUEPAD32_ROOT}/src/components/bluepad32 libbluepad32)
# Bluepad's example links arch_none, which SDK 2.2 implements using background
# IRQ work. Use a polling context instead so USB and Bluetooth share core 0's
# foreground, and BOOTSEL sampling cannot interrupt radio processing.
foreach(prop LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
    get_target_property(bp_links bluepad32 ${prop})
    list(REMOVE_ITEM bp_links pico_cyw43_arch_none)
    set_property(TARGET bluepad32 PROPERTY ${prop} "${bp_links}")
endforeach()
target_link_libraries(bluepad32 pico_cyw43_arch pico_async_context_poll)
target_compile_definitions(bluepad32 PUBLIC PICO_CYW43_ARCH_POLL=1 CYW43_LWIP=0
    PICO_FLASH_ASSUME_CORE1_SAFE=1)
add_executable(pico2w_receiver src/main.c src/platform.c src/bridge.c
    ${receiver_usb_sources} src/board_button.c src/ble_discovery.c src/battery.c
    src/receiver_config.c src/config_protocol.c src/config_flash.c)
target_link_libraries(pico2w_receiver PRIVATE pico_stdlib pico_unique_id
    pico_cyw43_arch pico_async_context_poll pico_btstack_ble pico_btstack_classic pico_btstack_cyw43
    hardware_watchdog hardware_flash pico_flash tinyusb_device bluepad32)
# Two config journal sectors, two Bluetooth sectors, and the final erratum
# sector are reserved; all application code must stay below these sectors.
target_compile_definitions(pico2w_receiver PRIVATE
    PICO_FLASH_BANK_STORAGE_OFFSET=0x3FD000 PICO_FLASH_BANK_TOTAL_SIZE=8192)
target_link_options(pico2w_receiver PRIVATE
    "-Wl,--wrap=uni_bt_le_on_gap_event_advertising_report"
    "-Wl,--wrap=uni_hid_parser_switch_parse_input_report"
    "-Wl,--wrap=uni_hid_parser_xboxone_parse_usage"
    "-Wl,--wrap=uni_bt_bredr_on_l2cap_incoming_connection"
    "-Wl,--defsym=__receiver_flash_limit=0x103FB000"
    "-Wl,-T,${CMAKE_CURRENT_LIST_DIR}/flash_guard.ld")
pico_enable_stdio_usb(pico2w_receiver 0)
pico_enable_stdio_uart(pico2w_receiver 0)
if(RECEIVER_USB_MODE STREQUAL "switch")
    target_compile_definitions(pico2w_receiver PRIVATE RECEIVER_USB_SWITCH=1)
    pico_set_program_name(pico2w_receiver "Pico 2 W Switch Receiver")
    pico_set_program_version(pico2w_receiver "Switch-R1.1")
else()
    pico_set_program_name(pico2w_receiver "Pico 2 W XInput Receiver")
    pico_set_program_version(pico2w_receiver "R5.1")
endif()
pico_add_extra_outputs(pico2w_receiver)
