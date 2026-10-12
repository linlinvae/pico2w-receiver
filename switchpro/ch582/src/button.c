/********************************************
 * Stick — CH592F Stadia USB Dongle
 * BOOT 按钮 (PB22) — 消抖 + 边缘事件
 *
 * 参考:
 *   sdk/EVT/EXAM/BLE/HAL/include/KEY.h — 按键宏定义风格
 ********************************************/

#include "button.h"
#include <string.h>
#include "button_gesture.h"
#include "board_config.h"
#ifdef CFG_CH582
#include "CH58x_common.h"
#include "CH58x_gpio.h"
#else
#include "CH59x_common.h"
#include "CH59x_gpio.h"
#endif

#if BOARD_BUTTON_PORT_A
#define BUTTON_READ GPIOA_ReadPortPin
#define BUTTON_MODE GPIOA_ModeCfg
#else
#define BUTTON_READ GPIOB_ReadPortPin
#define BUTTON_MODE GPIOB_ModeCfg
#endif

/* 消抖时间 (ms, 定义于 button.h) */

/* 外部 SysTick 毫秒计数器 (定义于 main.c) */
extern volatile uint32_t g_sys_tick_ms;

static bool s_last_raw = true;    /* 上一次原始电平 (true=高=松开) */
static bool s_debounced = true;   /* 消抖后电平 */
static bool s_event = false;      /* 短按释放事件，读取后清零 */
static bool s_long_event;
static button_gesture_t s_gesture;
static uint32_t s_last_change = 0;

void Button_Init(void)
{
    BUTTON_MODE(BOARD_BUTTON_PIN, BOARD_BUTTON_ACTIVE_LOW ? GPIO_ModeIN_PU : GPIO_ModeIN_PD);
    s_last_raw = true;
    s_debounced = true;
    s_event = false;
    s_long_event = false;
    memset(&s_gesture,0,sizeof(s_gesture));
    s_last_change = g_sys_tick_ms;
}

bool Button_IsPressed(void)
{
    return !s_debounced; /* 按下为低电平 */
}

bool Button_GetEvent(void)
{
    bool ev = s_event;
    s_event = false;
    return ev;
}

bool Button_GetLongEvent(void)
{
    bool event=s_long_event;
    s_long_event=false;
    return event;
}

void Button_Task(void)
{
    /* 读取当前电平 (PB22 按下为低) */
    bool raw = (BUTTON_READ(BOARD_BUTTON_PIN) != 0);
    if (!BOARD_BUTTON_ACTIVE_LOW) raw = !raw;

    /* 电平变化时记录时间 */
    if (raw != s_last_raw) {
        s_last_raw = raw;
        s_last_change = g_sys_tick_ms;
    }

    /* 稳定超过消抖时间 → 更新消抖后电平 */
    if ((g_sys_tick_ms - s_last_change) >= DEBOUNCE_MS) {
        if (s_debounced != s_last_raw) {
            s_debounced = s_last_raw;
        }
    }
    button_gesture_event_t event=button_gesture_update(&s_gesture,!s_debounced,g_sys_tick_ms);
    if (event==BUTTON_SHORT) s_event=true;
    else if (event==BUTTON_LONG) { s_long_event=true; s_event=false; }
}
