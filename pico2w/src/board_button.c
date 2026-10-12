/* Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 * SPDX-License-Identifier: BSD-3-Clause
 * Adapted from pico-examples/picoboard/button/button.c.
 * Core 1 must remain unused; no DMA may read XIP during this sample. */
#include "board_button.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
bool __no_inline_not_in_flash_func(receiver_bootsel_pressed)(void) {
    uint32_t irq = save_and_disable_interrupts();
    uint32_t previous = ioqspi_hw->io[1].ctrl;
    hw_write_masked(&ioqspi_hw->io[1].ctrl,
        GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
        IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
    for (volatile unsigned i = 0; i < 1000; ++i) { }
    bool pressed = !(sio_hw->gpio_hi_in & SIO_GPIO_HI_IN_QSPI_CSN_BITS);
    ioqspi_hw->io[1].ctrl = previous;
    restore_interrupts(irq); return pressed;
}
