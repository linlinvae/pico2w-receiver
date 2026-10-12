#ifndef BRIDGE_UART_H
#define BRIDGE_UART_H
#include <stdbool.h>
#include <stdint.h>
void bridge_uart_init(void);
bool bridge_uart_read(uint8_t *byte);
bool bridge_uart_overflow(void);
bool bridge_uart_tx_idle(void);
void bridge_uart_send(const uint8_t frame[24]);
void bridge_uart_poll(void);
#endif
