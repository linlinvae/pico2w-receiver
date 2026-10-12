/********************************************
 * Stick — CH592F Stadia USB Dongle
 * BLE Central + USB CDC/HID
 ********************************************/

#include <stdio.h>
#include <string.h>
#ifdef CFG_CH582
#include "CH58x_common.h"
#include "CH58x_gpio.h"
#include "CH58x_sys.h"
#include "CH58x_pwr.h"
#else
#include "CH59x_common.h"
#include "CH59x_gpio.h"
#include "CH59x_sys.h"
#include "CH59x_pwr.h"
#endif
#include "CONFIG.h"
#include "HAL.h"
#include "sinput_usb.h"
#include "sinput_builder.h"
#include "button.h"
#include "platform.h"
#include "log.h"
#include "stadia_ble.h"
#include "stadia_protocol.h"
#include "board_config.h"
#include "rumble_queue.h"
#include "xinput_builder.h"

__attribute__((aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];

/* ---- SysTick 轮询 ---- */
#define TICKS_PER_MS    60000
static volatile uint32_t g_last_systick = 0;
volatile uint32_t g_sys_tick_ms = 0;

static void update_tick(void)
{
    /*
     * SysTick->CNT 是 32-bit 自由运行计数器, 每 ~71.6s 从 0xFFFFFFFF 绕回 0.
     * 绕回后 now < g_last_systick, 直接用 now - g_last_systick 会 uint32_t 下溢.
     * 正确公式: elapsed = (2^32 - g_last_systick) + now, 等价于:
     *   (0xFFFFFFFF - g_last_systick) + now + 1
     */
    uint32_t now = (uint32_t)SysTick->CNT;
    uint32_t elapsed;
    if (now >= g_last_systick) {
        elapsed = now - g_last_systick;
    } else {
        elapsed = (0xFFFFFFFF - g_last_systick) + now + 1;
    }
    if (elapsed >= TICKS_PER_MS) {
        g_sys_tick_ms += elapsed / TICKS_PER_MS;
        g_last_systick = now - (elapsed % TICKS_PER_MS);
    }
}

/* ---- SInput Feature Request (SDL: 写 OUT [SINPUT_REPORT_ID_OUTPUT, SINPUT_CMD_FEATURES] → 回复 IN [SINPUT_REPORT_ID_FEATURES, ...]) ---- */
bool g_feature_req_pending = false;

static rumble_queue_t s_rumble;
static bool s_neutral_pending = true;

static uint16_t gamepad_build(uint32_t buttons, const uint8_t analog[6], uint8_t report[64])
{
#if USB_GAMEPAD_XINPUT
    xinput_build_report(buttons,analog,report);
    return XINPUT_REPORT_BYTES;
#else
    sinput_build_report(buttons,analog,g_stadia_battery,report);
    return 64;
#endif
}
static uint16_t gamepad_neutral(uint8_t report[64])
{
#if USB_GAMEPAD_XINPUT
    xinput_build_neutral_report(report);
    return XINPUT_REPORT_BYTES;
#else
    sinput_build_neutral_report(report);
    return 64;
#endif
}

/* ---- USB HID 输出回调 (只缓存, 不直接发 BLE) ---- */
static void on_hid_output(uint8_t *data, uint16_t len)
{
    /* Feature Request: data[0]=SINPUT_REPORT_ID_OUTPUT, data[1]=SINPUT_CMD_FEATURES */
    if (!USB_GAMEPAD_XINPUT && len >= 2 && data[0] == SINPUT_REPORT_ID_OUTPUT && data[1] == SINPUT_CMD_FEATURES) {
        LOG_INFO("HID", "Feature request from SDL");
        g_feature_req_pending = true;
        return;
    }
    /* 震动命令: 只缓存, 主循环发 (避免 GATT 操作冲突) */
    uint8_t left, right;
    bool rumble_valid = USB_GAMEPAD_XINPUT ? xinput_parse_rumble(data,len,&left,&right)
                                        : sinput_parse_haptic(data,len,&left,&right);
    if (g_stadia_connected && rumble_valid) {
        LOG_DEBUG("HID", "Rumble queued L=%u R=%u", left, right);
        rumble_queue_submit(&s_rumble, left, right);
    }
}

/* ---- BLE 状态回调 ---- */
void StadiaBLE_OnConnected(uint16_t conn_handle)
{
    LOG_INFO("BLE", "Controller connected, handle=%d", conn_handle);
    rumble_queue_reset(&s_rumble);
}

void StadiaBLE_OnDisconnected(uint16_t conn_handle)
{
    LOG_INFO("BLE", "Controller disconnected");
    rumble_queue_reset(&s_rumble);
    s_neutral_pending = true;
    (void)conn_handle;
}

static void delay_ms(uint32_t ms)
{
    uint32_t start = g_sys_tick_ms;
    while ((g_sys_tick_ms - start) < ms) {
        update_tick();
    }
}

#if STADIA_SCAN_DIAGNOSTICS
static uint32_t rtc_elapsed(uint32_t start, uint32_t end)
{
    /* Both supported SDKs wrap their RTC counter at 24 hours. */
    const uint32_t rtc_day = 86400UL * 32768UL;
    return end >= start ? end - start : rtc_day - start + end;
}
#endif

/* Only an explicitly selected external clock needs this bounded precheck before
 * SDK RTC_InitTime(). The default internal clock follows the upstream SDK path. */
static bool prepare_ble_clock(void)
{
#if CLK_OSC32K == 0
    sys_safe_access_enable();
    R8_CK32K_CONFIG |= RB_CLK_XT32K_PON | RB_CLK_OSC32K_XT;
    sys_safe_access_disable();
    uint8_t previous = R8_CK32K_CONFIG & RB_32K_CLK_PIN;
    unsigned edges = 0;
    uint32_t start = g_sys_tick_ms;
    while (g_sys_tick_ms - start < 1000) {
        update_tick();
        CDC_Flush();
        uint8_t current = R8_CK32K_CONFIG & RB_32K_CLK_PIN;
        if (current != previous) {
            previous = current;
            if (++edges >= 32) return true;
        }
    }
    LOG_ERROR("CLOCK", "External startup timeout edges=%u CK32K=0x%02X", edges, R8_CK32K_CONFIG);
    return false;
#else
    return true;
#endif
}

#if STADIA_SCAN_DIAGNOSTICS
static void log_running_clock(void)
{
    static uint32_t previous_ms, previous_rtc;
    static bool active;
    uint32_t now = g_sys_tick_ms;
    bool want = StadiaBLE_IsScanning() || g_stadia_connected;
    if (!want) {
        active = false;
        return;
    }
    uint32_t rtc = R32_RTC_CNT_32K;
    if (!active) {
        previous_ms = now;
        previous_rtc = rtc;
        active = true;
    } else if (now - previous_ms >= 1000) {
        uint32_t delta = rtc_elapsed(previous_rtc, rtc);
        LOG_INFO("CLOCK", "Running ms=%lu rtc-ticks=%lu CK32K=0x%02X",
                 (unsigned long)(now - previous_ms), (unsigned long)delta, R8_CK32K_CONFIG);
        previous_ms = now;
        previous_rtc = rtc;
    }
}
#endif

int main(void)
{
    PWR_DCDCCfg(BOARD_DCDC_ENABLED ? ENABLE : DISABLE);
    SetSysClock(CLK_SOURCE_PLL_60MHz);

    platform_board_init();
    platform_led_on();

    /* === USB 先 (日志) === */
    log_init();
    Button_Init();
    SInputUSB_Init();
    SInputUSB_SetOutputCallback(on_hid_output);

    /* SysTick 配置 (给 delay_ms 和日志时间戳用) */
    SysTick_Config(SysTick_LOAD_RELOAD_Msk);
    PFIC_DisableIRQ(SysTick_IRQn);
    g_last_systick = (uint32_t)SysTick->CNT;

    delay_ms(200); /* 等 USB 枚举 */
    CDC_Flush();

    LOG_INFO("MAIN", "=== BLE Central init ===");
    LOG_INFO("MAIN", "Board=%s LED=%d DCDC=%d", BOARD_NAME,
             BOARD_LED_ENABLED, BOARD_DCDC_ENABLED);
    LOG_INFO("MAIN", "USB mode=%s",USB_GAMEPAD_XINPUT ? "XInput" : "SInput");
#ifdef CFG_CH582
    const uint8_t expected_chip_id = 0x82;
#else
    const uint8_t expected_chip_id = 0x92;
#endif
    uint8_t actual_chip_id = R8_CHIP_ID;
    LOG_INFO("MAIN", "ChipID=0x%02X expected=0x%02X", actual_chip_id, expected_chip_id);
    LOG_INFO("MAIN", "Reset cause=0x%02X (0=software 1=power-on 2=watchdog 3=RST)",
             R8_RESET_STATUS & RB_RESET_FLAG);
    LOG_INFO("MAIN", "ScanDiag=%d Button=%c mask=0x%08lX active-low=%d",
             STADIA_SCAN_DIAGNOSTICS, BOARD_BUTTON_PORT_A ? 'A' : 'B',
             (unsigned long)BOARD_BUTTON_PIN, BOARD_BUTTON_ACTIVE_LOW);
#if STADIA_SCAN_DIAGNOSTICS
    LOG_INFO("MAIN", "ScanDiagRev=%d", STADIA_SCAN_DIAG_REVISION);
#endif
    CDC_Flush();

    /* === BLE 协议栈 (Central) === */
    if (actual_chip_id != expected_chip_id) {
        LOG_ERROR("MAIN", "Chip mismatch; flash firmware for the actual chip. BLE init stopped");
        while (1) {
            update_tick();
            CDC_Flush();
        }
    }
    LOG_INFO("CLOCK", "RTC source=%u (0=external-32768, 1=internal-32000, 2=internal-32768)",
             CLK_OSC32K);
    if (!prepare_ble_clock()) {
        LOG_ERROR("CLOCK", "External 32K not stable; BLE init stopped, check crystal circuit");
        while (1) {
            update_tick();
            CDC_Flush();
        }
    }
#ifdef CFG_CH582
    CH58X_BLEInit();
    LOG_INFO("MAIN", "CH58X_BLEInit OK");
#else
    CH59x_BLEInit();
    LOG_INFO("MAIN", "CH59x_BLEInit OK");
#endif
    /* BLEInit 内部调用 SysTick_Config 重置了计数器, 重新同步基准 */
    g_last_systick = (uint32_t)SysTick->CNT;
    CDC_Flush();

    HAL_Init();
    LOG_INFO("MAIN", "HAL_Init OK");
    CDC_Flush();

    /* 看门狗: BLE 初始化完成后使能, 避开 BLE 慢启动窗口 */
    platform_wdog_init();

    GAPRole_CentralInit();
    LOG_INFO("MAIN", "GAPRole_CentralInit OK");
    CDC_Flush();

    StadiaBLE_Init();
    LOG_INFO("MAIN", "StadiaBLE_Init OK, press BOOT to scan");
    CDC_Flush();

    /* === 主循环 === */
    uint32_t last_led = 0;

    while (1) {
        update_tick();
        platform_wdog_feed();
        uint32_t now = g_sys_tick_ms;

        CDC_Flush();

        /* 处理 HID 输出报告 (震动/Feature请求) */
        SInputUSB_ProcessOutput();

        /* 按钮: BOOT 控制扫描 (仅 BLE 就绪后生效) */
        Button_Task();
        if (Button_GetLongEvent()) {
            if (g_ble_ready) StadiaBLE_ForgetPeer();
            else LOG_WARN("BTN", "BLE not ready yet");
        } else if (Button_GetEvent()) {
            if (!g_ble_ready) {
                LOG_WARN("BTN", "BLE not ready yet");
                CDC_Flush();
            } else if (g_stadia_connected) {
                LOG_INFO("BTN", "BOOT: disconnect + scan");
                StadiaBLE_StartScan();
            } else if (StadiaBLE_IsScanning()) {
                LOG_INFO("BTN", "BOOT: stop scan");
                StadiaBLE_StopScan();
            } else {
                LOG_INFO("BTN", "BOOT: start scan");
                StadiaBLE_StartScan();
            }
            CDC_Flush();
        }

        /* SInput Feature 响应: 当 SDL 请求能力时, 发送 SINPUT_REPORT_ID_FEATURES 报告 */
        if (!USB_GAMEPAD_XINPUT && g_feature_req_pending) {
            uint8_t uid[6];
            platform_get_unique_id(uid);
            uint8_t resp[63];
            sinput_build_feature_response(uid, resp);
            uint8_t report[64];
            report[0] = SINPUT_REPORT_ID_FEATURES;
            memcpy(report + 1, resp, 63);
            if (!SInputUSB_SendReport(report, 64)) {
                /* EP3 忙: 保留 pending 标志, 下一轮主循环重试 (同输入报告 ready 机制) */
                /* Keep pending without flooding CDC while the host is busy. */
            } else {
                g_feature_req_pending = false;  /* 发送成功才清除 */
                LOG_INFO("HID", "Feature response sent");
            }
            CDC_Flush();
        }

        /* LED: 初始化=常亮, IDLE=慢闪 500ms, 扫描=快闪 100ms, 已连接=灭 */
        {
            static uint8_t led_state = 99;  /* 初始值确保首次必然更新 */
            uint8_t want;
            if (!g_ble_ready)                want = 3;  /* 初始化: 常亮 */
            else if (g_stadia_connected)     want = 2;  /* 已连接: 灭 */
            else if (StadiaBLE_IsScanning()) want = 1;  /* 扫描: 快闪 */
            else                             want = 0;  /* 空闲: 慢闪 */

            if (want != led_state) {
                led_state = want;
                last_led = now;
                if (want == 3)      platform_led_on();
                else if (want == 2) platform_led_off();
                /* want 0/1: 下面靠翻转维持闪烁 */
            }

            if (led_state <= 1) {
                uint16_t interval = (led_state == 1) ? 100 : 500;
                if ((now - last_led) >= interval) {
                    last_led = now;
                    platform_led_toggle();
                }
            }
        }

        TMOS_SystemProcess();
#if STADIA_SCAN_DIAGNOSTICS
        log_running_clock();
#endif

        /* Discovery owns its progress deadline; slow valid replies must not be
         * disconnected just because three seconds passed since connection. */
        StadiaBLE_Poll();

        /* Release stale host inputs even after BLE disconnects.
         * Retry a busy USB endpoint, before forwarding any new controller input. */
        if (s_neutral_pending) {
            uint8_t report[64];
            uint16_t bytes=gamepad_neutral(report);
            if (SInputUSB_SendReport(report, bytes))
                s_neutral_pending = false;
        }

        /* Stadia 输入报告 → SInput USB 发送 */
        if (!s_neutral_pending && g_stadia_report_ready && g_stadia_connected) {
            uint32_t buttons;
            uint8_t analog[6];
            if (stadia_parse_report(g_stadia_raw_data, g_stadia_raw_len,
                                     &buttons, analog)) {
                uint8_t report[64];
                uint16_t bytes=gamepad_build(buttons,analog,report);
                if (!SInputUSB_SendReport(report, bytes)) {
                    /* EP3 忙: 保留 ready 标志, 下一轮主循环用最新数据重试.
                     * 如果此日志频繁出现, 说明 USB 是瓶颈; 如果不出现但摇杆仍然卡偏移, 说明是 BLE 丢包 */
                    LOG_DEBUG("HID", "Input report dropped (EP3 busy), will retry");
                } else {
                    g_stadia_report_ready = false;
                }
            } else {
                g_stadia_report_ready = false;
            }
        }

        /* Pending neutral/fresh input has priority over an idle refresh. */
        if (!s_neutral_pending && !(g_stadia_connected && g_stadia_report_ready))
            SInputUSB_Poll(now);

        /* Latest value, stop priority, and wrap-safe retry scheduling. */
        if (g_stadia_connected && g_hid_output_handle > 0 &&
            StadiaBLE_IsOutputReady() && rumble_queue_ready(&s_rumble, now)) {
            uint8_t left, right, rumble[4];
            rumble_queue_value(&s_rumble, &left, &right);
            stadia_build_rumble_report(left, right, rumble);
            bool success = StadiaBLE_SendOutput(rumble, sizeof(rumble));
            rumble_queue_result(&s_rumble, now, success);
            if (!success) {
                if (s_rumble.failures >= RUMBLE_FAILURE_LIMIT) {
                    LOG_WARN("HID", "Rumble failures=%u, disconnecting; press BOOT to retry",
                             s_rumble.failures);
                    StadiaBLE_Disconnect();
                    rumble_queue_reset(&s_rumble);
                } else {
                    LOG_WARN("HID", "Rumble fail #%u, retry in %ums",
                             s_rumble.failures, s_rumble.wait_ms);
                }
            }
        }
        /* 周期电池读取 + 日志: 连接后每 60s 读取并打印 */
        {
            static uint32_t s_last_bat_poll = 0;
            if (g_stadia_connected && (now - s_last_bat_poll >= 60000)) {
                s_last_bat_poll = now;
                if (g_stadia_bat_handle > 0)
                    StadiaBLE_ReadBattery();
                /* ReadBattery 会异步更新 g_stadia_battery, 这里仅打印当前值 */
                if (g_stadia_battery <= 100)
                    LOG_INFO("BAT", "Last: %d%%", g_stadia_battery);
                else
                    LOG_INFO("BAT", "Last: unknown");
            }
        }
    }
}
