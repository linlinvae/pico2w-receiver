// SPDX-License-Identifier: Apache-2.0
#include "receiver_config.h"
#include "hardware/flash.h"
#include "pico/flash.h"
#include "pico/stdlib.h"
#include <string.h>
static int active=-1;
static uint8_t page[FLASH_PAGE_SIZE] __attribute__((aligned(4)));
static uint32_t target;
static const uint8_t *bank(unsigned index) { return (const uint8_t *)(XIP_BASE+(index?CONFIG_FLASH_B:CONFIG_FLASH_A)); }
bool config_flash_load(receiver_config_t *c,uint32_t *generation) {
    receiver_config_t a,b;uint32_t ga=0,gb=0;
    bool va=config_record_read(bank(0),&a,&ga),vb=config_record_read(bank(1),&b,&gb);
    active=va && (!vb || (int32_t)(ga-gb)>0)?0:vb?1:-1;
    if(active<0) { config_defaults(c);*generation=0;return false; }
    *c=active?b:a;*generation=active?gb:ga;return true;
}
static void __not_in_flash_func(write_page)(void *unused) {
    (void)unused;flash_range_erase(target,FLASH_SECTOR_SIZE);flash_range_program(target,page,FLASH_PAGE_SIZE);
}
bool config_flash_save(const receiver_config_t *c,uint32_t *generation) {
    if(!config_valid(c))return false;
    uint8_t encoded[CONFIG_SIZE];config_encode(c,encoded);
    if(active>=0 && !memcmp(bank((unsigned)active)+8,encoded,CONFIG_SIZE))return true;
    unsigned next=active==0?1:0;target=next?CONFIG_FLASH_B:CONFIG_FLASH_A;
    config_record_make(page,c,*generation+1);
    if(flash_safe_execute(write_page,NULL,1000)!=PICO_OK)return false;
    receiver_config_t readback;uint32_t g;
    if(!config_record_read(bank(next),&readback,&g) || g!=*generation+1 || memcmp(bank(next)+8,encoded,CONFIG_SIZE))return false;
    active=(int)next;*generation=g;return true;
}
