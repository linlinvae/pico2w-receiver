#ifndef __SINPUT_USB_H__
#define __SINPUT_USB_H__

#include <stdint.h>
#include <stdbool.h>

#include <stdio.h>

/* 端点大小 */
#define EP0_SIZE     64
#define EP2_SIZE     64   /* CDC Data IN/OUT (批量) */
#define EP3_SIZE     64   /* Gamepad DMA capacity; XUSB endpoint max packet is 32 */

/* CDC 发送环形缓冲大小 */
#define CDC_TX_BUF_SIZE  1024

/*
 * USB 复合设备: CDC + 游戏手柄。默认 SInput HID；USB_GAMEPAD_XINPUT=1
 * 使用 PC wired XUSB 1.00。保留 SInputUSB_ API 名称以共用原生 WCH 传输层。
 *
 * 端点分配:
 *   EP1 IN — CDC 通知 (中断)
 *   EP2 IN/OUT — CDC 数据 (批量)
 *   EP3 IN/OUT — SInput / XUSB 报告 (中断)
 *
 * 参考:
 *   sdk/EVT/EXAM/USB/Device/HID_CompliantDev/src/Main.c  — USB 框架
 *   sdk/EVT/EXAM/USB/Device/COM/src/Main.c               — CDC 框架
 *   sdk/EVT/EXAM/SRC/StdPeriphDriver/CH59x_usbdev.c       — USB 驱动
 */

/**
 * @brief   初始化 USB 设备控制器 (CDC + 游戏手柄)
 *          配置端点、DMA 缓冲区、中断。初始化后 USB 开始枚举。
 */
void SInputUSB_Init(void);

/**
 * @brief   通过 EP3 IN 发送 SInput (64 字节) 或 XUSB (20 字节) 报告
 * @param   data    报告数据
 * @param   len     数据长度（最大 64）
 * @return  true    发送成功 (EP3 已置 ACK 等待主机轮询)
 *          false   USB 未就绪或 EP3 忙，调用方保留报告重试
 *                  输入报告调用方应保留 ready 标志在下一轮重试
 */
bool SInputUSB_SendReport(uint8_t *data, uint16_t len);

/**
 * @brief   注册 EP3 OUT 回调（收到主机输出报告时调用，如震动命令）
 * @param   cb      回调函数指针
 */
void SInputUSB_SetOutputCallback(void (*cb)(uint8_t *data, uint16_t len));

/**
 * @brief   刷新 CDC 发送缓冲（在主循环中调用）
 */
void CDC_Flush(void);

/**
 * @brief   USB 是否已枚举就绪
 */
bool SInputUSB_IsReady(void);

/**
 * @brief   处理缓存的 HID 输出报告（在主循环中调用）
 *          USB ISR 只存数据设标志，延迟到主循环处理，
 *          避免在中断中调用 BLE API 或 LOG。
 */
void SInputUSB_ProcessOutput(void);
/* XInput variant: periodically refresh its 20-byte wired report. */
void SInputUSB_Poll(uint32_t now);

#endif /* __SINPUT_USB_H__ */
