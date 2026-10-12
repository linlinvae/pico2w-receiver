#include "CH58x_common.h"
#include "board_config.h"
#include "bridge_uart.h"
#include <string.h>
#define RX_SIZE 256
static volatile uint16_t rx_head, rx_tail;
static volatile bool overflow;
static uint8_t rx[RX_SIZE], tx[24];
static uint8_t tx_pos = 24;
void bridge_uart_init(void) {
#if BOARD_UART_REMAP
    GPIOPinRemap(ENABLE, RB_PIN_UART1);
    GPIOB_SetBits(GPIO_Pin_13);
    GPIOB_ModeCfg(GPIO_Pin_12, GPIO_ModeIN_PU);
    GPIOB_ModeCfg(GPIO_Pin_13, GPIO_ModeOut_PP_5mA);
#else
    GPIOPinRemap(DISABLE, RB_PIN_UART1);
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
#endif
    UART1_DefInit(); UART1_BaudRateCfg(BOARD_UART_BAUD);
    UART1_ByteTrigCfg(UART_4BYTE_TRIG);
    UART1_INTCfg(ENABLE, RB_IER_RECV_RDY | RB_IER_LINE_STAT);
    PFIC_EnableIRQ(UART1_IRQn);
}
__INTERRUPT __HIGH_CODE void UART1_IRQHandler(void) {
    uint8_t status = R8_UART1_LSR;
    if (status & (RB_LSR_OVER_ERR | RB_LSR_FRAME_ERR | RB_LSR_PAR_ERR | RB_LSR_BREAK_ERR)) overflow = true;
    while (R8_UART1_RFC) {
        uint8_t byte = UART1_RecvByte();
        uint16_t next = (rx_head + 1) & (RX_SIZE - 1);
        if (next == rx_tail) overflow = true;
        else { rx[rx_head] = byte; rx_head = next; }
    }
    (void)UART1_GetITFlag();
}
bool bridge_uart_read(uint8_t *byte) {
    if (rx_tail == rx_head) return false;
    *byte = rx[rx_tail]; rx_tail = (rx_tail + 1) & (RX_SIZE - 1); return true;
}
bool bridge_uart_overflow(void) {
    PFIC_DisableIRQ(UART1_IRQn);
    bool result = overflow;
    if (result) { rx_tail = rx_head; overflow = false; }
    PFIC_EnableIRQ(UART1_IRQn); return result;
}
bool bridge_uart_tx_idle(void) { return tx_pos == 24; }
void bridge_uart_send(const uint8_t frame[24]) { memcpy(tx, frame, 24); tx_pos = 0; }
void bridge_uart_poll(void) {
    while (tx_pos < 24 && R8_UART1_TFC < UART_FIFO_SIZE) UART1_SendByte(tx[tx_pos++]);
}
