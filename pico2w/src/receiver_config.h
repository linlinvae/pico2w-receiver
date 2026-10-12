// SPDX-License-Identifier: Apache-2.0
#ifndef RECEIVER_CONFIG_H
#define RECEIVER_CONFIG_H
#include <stdbool.h>
#include <stdint.h>
#define CONFIG_SIZE 128
#define CONFIG_BUTTONS 15
#define CONFIG_STEPS 16
#define CONFIG_FLASH_A 0x3FB000u
#define CONFIG_FLASH_B 0x3FC000u
typedef struct { uint16_t buttons, duration; uint8_t lt, rt; } config_step_t;
typedef struct {
    uint8_t invert, deadzone[2], rumble[2], trigger, count, map[CONFIG_BUTTONS];
    config_step_t steps[CONFIG_STEPS];
} receiver_config_t;
extern const uint16_t config_button_bits[CONFIG_BUTTONS];
uint32_t config_crc32(const uint8_t *data, unsigned size);
uint16_t config_crc16(const uint8_t *data, unsigned size);
void config_defaults(receiver_config_t *c);
bool config_valid(const receiver_config_t *c);
void config_encode(const receiver_config_t *c, uint8_t out[CONFIG_SIZE]);
bool config_decode(receiver_config_t *c, const uint8_t data[CONFIG_SIZE]);
bool config_record_read(const uint8_t page[256], receiver_config_t *c, uint32_t *generation);
void config_record_make(uint8_t page[256], const receiver_config_t *c, uint32_t generation);
bool config_flash_load(receiver_config_t *c, uint32_t *generation);
bool config_flash_save(const receiver_config_t *c, uint32_t *generation);
#endif
