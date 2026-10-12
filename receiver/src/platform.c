/********************************************
 * Stick — CH592F / CH582F Stadia USB Dongle
 * 平台抽象层
 *
 * 参考:
 *   CH5x8_sys.c                          — mDelaymS, SYS_GetSysTickCnt
 *   CH5x8 数据手册                        — 芯片唯一 ID 地址
 *   sdk/.../BLE/HAL/include/LED.h        — LED 宏定义风格
 ********************************************/

#include "platform.h"
#include "board_config.h"

#ifdef CFG_CH582
#include "CH58x_common.h"
#include "CH58x_gpio.h"
#include "CH58x_sys.h"
#else
#include "CH59x_common.h"
#include "CH59x_gpio.h"
#include "CH59x_sys.h"
#endif

/*********************************************************************
 * LED 控制 (PA8, 低电平点亮)
 *********************************************************************/
#if BOARD_LED_PORT_A
#define LED_SET GPIOA_SetBits
#define LED_RESET GPIOA_ResetBits
#define LED_TOGGLE GPIOA_InverseBits
#define LED_MODE GPIOA_ModeCfg
#else
#define LED_SET GPIOB_SetBits
#define LED_RESET GPIOB_ResetBits
#define LED_TOGGLE GPIOB_InverseBits
#define LED_MODE GPIOB_ModeCfg
#endif

void platform_board_init(void)
{
    /* Initialize only owned pins; leave USB, RF and debug pins alone. */
#if BOARD_LED_ENABLED
    platform_led_off();
    LED_MODE(BOARD_LED_PIN, GPIO_ModeOut_PP_5mA);
#endif
}

void platform_led_on(void)
{
#if BOARD_LED_ENABLED
#if BOARD_LED_ACTIVE_LOW
    LED_RESET(BOARD_LED_PIN);
#else
    LED_SET(BOARD_LED_PIN);
#endif
#endif
}

void platform_led_off(void)
{
#if BOARD_LED_ENABLED
#if BOARD_LED_ACTIVE_LOW
    LED_SET(BOARD_LED_PIN);
#else
    LED_RESET(BOARD_LED_PIN);
#endif
#endif
}

void platform_led_toggle(void)
{
#if BOARD_LED_ENABLED
    LED_TOGGLE(BOARD_LED_PIN);
#endif
}

/*********************************************************************
 * 系统时间
 * 参考: MCU.c SYS_GetSysTickCnt()
 * 注意: 需要在主循环中调用 update_tick() 刷新 g_sys_tick_ms
 *********************************************************************/
uint32_t platform_time_ms(void)
{
    return g_sys_tick_ms;
}

/*********************************************************************
 * 芯片唯一标识：使用对应 SDK 的出厂 MAC 读取接口。
 * 不假设 CH582/CH592 的 ROM 信息地址相同。
 *********************************************************************/
void platform_get_unique_id(uint8_t buf[6])
{
    memset(buf, 0, 6);
    GetMACAddress(buf);
}

/*********************************************************************
 * 软件复位
 *********************************************************************/
void platform_reboot(void)
{
    sys_safe_access_enable();
    R8_RST_WDOG_CTRL |= RB_SOFTWARE_RESET;
    sys_safe_access_disable();
    while (1);
}

/*********************************************************************
 * 看门狗 (WWDG)
 * 溢出窗口 ~437ms (60MHz/131072≈457.8Hz, 计数器 200)
 * 使能溢出复位; 主循环每轮喂狗, 防 BLE ATT 死锁
 *********************************************************************/
void platform_wdog_init(void)
{
    sys_safe_access_enable();
    WWDG_SetCounter(200);
    R8_RST_WDOG_CTRL |= RB_WDOG_RST_EN;
    sys_safe_access_disable();
}

void platform_wdog_feed(void)
{
    WWDG_SetCounter(200);
}

/*********************************************************************
 * 阻塞延时
 *********************************************************************/
void platform_delay_ms(uint32_t ms)
{
    mDelaymS(ms);
}
