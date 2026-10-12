#include "CH58x_common.h"
#include "board_config.h"
#include "button.h"
#include "bridge_uart.h"
#include "log.h"
#include "platform.h"
#include "sinput_usb.h"
#include "sp_bridge.h"
#include "xinput_builder.h"

volatile uint32_t g_sys_tick_ms;
static uint32_t last_tick;
static sp_bridge_t bridge;
static sp_decoder_t decoder;
static void tick(void) {
    uint32_t now = (uint32_t)SysTick->CNT;
    uint32_t delta = now - last_tick;
    uint32_t ms = delta / 60000;
    g_sys_tick_ms += ms; last_tick += ms * 60000;
}
static void output(uint8_t *data, uint16_t size) {
    uint8_t left, right;
    if (SInputUSB_IsReady() && xinput_parse_rumble(data, size, &left, &right))
        sp_bridge_rumble(&bridge, left, right);
}
int main(void) {
    SetSysClock(CLK_SOURCE_PLL_60MHz);
    PWR_DCDCCfg(BOARD_DCDC_ENABLED ? ENABLE : DISABLE);
    platform_board_init(); log_init(); Button_Init();
    sp_bridge_init(&bridge);
    SInputUSB_Init(); SInputUSB_SetOutputCallback(output);
    SysTick_Config(SysTick_LOAD_RELOAD_Msk); PFIC_DisableIRQ(SysTick_IRQn);
    last_tick = (uint32_t)SysTick->CNT;
    if (R8_CHIP_ID != 0x82) {
        LOG_ERROR("MAIN", "Wrong chip, expected CH582F; UART disabled");
        for (;;) { tick(); CDC_Flush(); }
    }
    bridge_uart_init(); platform_wdog_init();
    LOG_INFO("MAIN", "%s; ESP32 classic Bluetooth module required", BOARD_NAME);
    bool previous_usb = false, previous_controller = false;
    for (;;) {
        tick(); platform_wdog_feed();
        uint32_t now = g_sys_tick_ms;
        if (bridge_uart_overflow()) {
            decoder.used = 0;
            /* Fail closed after known byte loss, then wait for a fresh input frame. */
            sp_bridge_poll(&bridge, bridge.last_rx + SP_LINK_TIMEOUT_MS);
        }
        uint8_t byte; sp_frame_t frame;
        for (unsigned i = 0; i < 256 && bridge_uart_read(&byte); ++i)
            if (sp_decode_byte(&decoder, byte, &frame)) sp_bridge_receive(&bridge, &frame, now);
        sp_bridge_poll(&bridge, now);
        bool usb = SInputUSB_IsReady();
        if (previous_usb && !usb) {
            sp_bridge_rumble(&bridge, 0, 0); bridge.neutral_pending = true;
        }
        previous_usb = usb;
        SInputUSB_ProcessOutput();
        Button_Task();
        if (Button_GetLongEvent()) sp_bridge_control(&bridge, SP_OP_FORGET);
        if (Button_GetEvent()) sp_bridge_control(&bridge, SP_OP_SCAN);
        uint8_t report[20];
        if (bridge.neutral_pending) {
            xinput_build_neutral_report(report);
            if (SInputUSB_SendReport(report, 20)) bridge.neutral_pending = false;
        } else if (bridge.input_pending) {
            sp_xinput_report(&bridge.input, report);
            if (SInputUSB_SendReport(report, 20)) bridge.input_pending = false;
        }
        if (bridge_uart_tx_idle() && sp_bridge_tx(&bridge, now, &frame)) {
            uint8_t wire[24]; sp_encode(&frame, wire); bridge_uart_send(wire);
        }
        bridge_uart_poll(); SInputUSB_Poll(now); CDC_Flush();
        if (previous_controller != bridge.input.connected) {
            previous_controller = bridge.input.connected;
            LOG_INFO("LINK", "Switch Pro %s", previous_controller ? "connected" : "disconnected/stale");
        }
    }
}
