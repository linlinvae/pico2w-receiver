#ifndef TEST_FLASH_H
#define TEST_FLASH_H
#include <stdint.h>
#include <stddef.h>
#define FLASH_PAGE_SIZE 256
#define FLASH_SECTOR_SIZE 4096
extern uint8_t config_test_flash[4*1024*1024];
#define XIP_BASE ((uintptr_t)config_test_flash)
void flash_range_erase(uint32_t offset,size_t length);
void flash_range_program(uint32_t offset,const uint8_t *data,size_t length);
#endif
