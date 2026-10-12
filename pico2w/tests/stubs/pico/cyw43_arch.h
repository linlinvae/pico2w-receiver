#ifndef TEST_CYW43_ARCH_H
#define TEST_CYW43_ARCH_H
#include <stdbool.h>
#define CYW43_WL_GPIO_LED_PIN 0
void cyw43_arch_gpio_put(unsigned pin, bool value);
#endif
