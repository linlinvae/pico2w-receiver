#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#ifndef STADIA_SCAN_DIAGNOSTICS
#define STADIA_SCAN_DIAGNOSTICS 0
#endif
#define STADIA_SCAN_DIAG_REVISION 11

/* Separate firmware variants share BLE/bond storage, but expose one gamepad. */
#ifndef USB_GAMEPAD_XINPUT
#define USB_GAMEPAD_XINPUT 0
#endif

/* App journal uses two 256-byte DataFlash pages, below both SDK SNV defaults. */
#ifndef BOARD_PEER_STORE_ADDR
#define BOARD_PEER_STORE_ADDR 0x6E00
#endif
#ifndef BLE_SNV
#define BLE_SNV 1
#endif

/* Match the unmodified upstream build's SDK default: calibrated internal 32K.
 * Force-include this file so application, HAL and drivers agree. External 32K
 * remains an explicit board override, not a prerequisite for this dev board. */
#ifndef BOARD_LF_CLOCK_SOURCE
#define BOARD_LF_CLOCK_SOURCE 1
#endif
#if BOARD_LF_CLOCK_SOURCE < 0 || BOARD_LF_CLOCK_SOURCE > 2
#error Unsupported low-frequency clock source
#endif
#ifndef CLK_OSC32K
#define CLK_OSC32K BOARD_LF_CLOCK_SOURCE
#endif

/* Override these at build time after checking the board schematic.
 * The photograph does not establish an LED connection or polarity. */
#ifndef BOARD_NAME
#ifdef BOARD_CUSTOM
#define BOARD_NAME "custom-receiver"
#else
#ifdef CFG_CH582
#define BOARD_NAME "CH582F-dev"
#else
#define BOARD_NAME "CH592F-dev"
#endif
#endif
#endif
#ifndef BOARD_LED_ENABLED
#define BOARD_LED_ENABLED 0
#endif
#ifndef BOARD_LED_PORT_A
#define BOARD_LED_PORT_A 1
#endif
#ifndef BOARD_LED_PIN
#define BOARD_LED_PIN GPIO_Pin_8
#endif
#ifndef BOARD_LED_ACTIVE_LOW
#define BOARD_LED_ACTIVE_LOW 1
#endif
#ifndef BOARD_BUTTON_PORT_A
#define BOARD_BUTTON_PORT_A 0
#endif
#ifndef BOARD_BUTTON_PIN
#define BOARD_BUTTON_PIN GPIO_Pin_22
#endif
#ifndef BOARD_BUTTON_ACTIVE_LOW
#define BOARD_BUTTON_ACTIVE_LOW 1
#endif
/* Keep the internal DC/DC off unless the external circuit is confirmed. */
#ifndef BOARD_DCDC_ENABLED
#define BOARD_DCDC_ENABLED 0
#endif

#endif
