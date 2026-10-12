// SPDX-License-Identifier: Apache-2.0
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "uni.h"
#include "platform.h"
#if !PICO_RP2350 || !PICO_CYW43_ARCH_POLL || PICO_CYW43_ARCH_THREADSAFE_BACKGROUND
#error "Receiver requires RP2350 and foreground CYW43 polling"
#endif
int main(void) {
    /* One foreground run loop owns Bluetooth, USB and application state. */
    stdio_init_all();
    if (cyw43_arch_init()) return 1;
    uni_platform_set_custom(receiver_platform());
    uni_init(0, NULL);
    btstack_run_loop_execute();
    return 0;
}
