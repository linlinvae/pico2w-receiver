// SPDX-License-Identifier: Apache-2.0
#include "config_protocol.h"
#include "hardware/flash.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned checks,writes;
static int fail_safe,partial=256;
uint8_t config_test_flash[4*1024*1024];
#define CHECK(x) do { ++checks;assert(x); } while(0)
void flash_range_erase(uint32_t offset,size_t size) { CHECK((offset==CONFIG_FLASH_A || offset==CONFIG_FLASH_B) && size==4096);memset(config_test_flash+offset,255,size); }
void flash_range_program(uint32_t offset,const uint8_t *p,size_t size) { CHECK((offset==CONFIG_FLASH_A || offset==CONFIG_FLASH_B) && size==256);memcpy(config_test_flash+offset,p,(unsigned)partial);writes++; }
int flash_safe_execute(void (*fn)(void *),void *arg,uint32_t timeout) { CHECK(timeout==1000);if(fail_safe)return -1;fn(arg);return 0; }
static void command(config_protocol_t *p,unsigned op,unsigned token,unsigned offset,const uint8_t *data,unsigned size) {
    uint8_t q[31]={ 'P','2','C',1 };q[4]=(uint8_t)op;q[5]=(uint8_t)token;q[6]=(uint8_t)(token>>8);q[7]=(uint8_t)offset;q[8]=(uint8_t)size;
    if(data)memcpy(q+10,data,size);
    uint16_t crc=config_crc16(q,29);q[29]=(uint8_t)crc;q[30]=(uint8_t)(crc>>8);config_protocol_set(p,q,31);
}
static uint16_t buttons(bridge_t *b) { return (uint16_t)(b->report[2]|b->report[3]<<8); }
int main(int argc,char **argv) {
    receiver_config_t c,d;config_defaults(&c);uint8_t bytes[128],page[256];uint32_t generation;
    CHECK(config_valid(&c));CHECK(config_crc32((const uint8_t *)"123456789",9)==0xcbf43926u);CHECK(config_crc16((const uint8_t *)"123456789",9)==0x29b1);
    config_encode(&c,bytes);CHECK(config_decode(&d,bytes));CHECK(d.trigger==255 && d.rumble[0]==100 && d.map[14]==14);
    for(unsigned i=0;i<128;i++) { uint8_t bad[128];memcpy(bad,bytes,128);bad[i]^=1;CHECK(!config_decode(&d,bad)); }
    c.deadzone[0]=51;CHECK(!config_valid(&c));c.deadzone[0]=0;c.map[0]=15;CHECK(!config_valid(&c));config_defaults(&c);
    config_record_make(page,&c,5);CHECK(config_record_read(page,&d,&generation) && generation==5);page[140]=0;CHECK(!config_record_read(page,&d,&generation));
    memset(config_test_flash,255,sizeof(config_test_flash));CHECK(!config_flash_load(&d,&generation) && generation==0);
    CHECK(config_flash_save(&c,&generation) && generation==1);unsigned before=writes;CHECK(config_flash_save(&c,&generation) && writes==before);
    c.map[11]=12;partial=100;CHECK(!config_flash_save(&c,&generation));CHECK(config_flash_load(&d,&generation) && generation==1 && d.map[11]==11);
    partial=256;CHECK(config_flash_save(&c,&generation) && generation==2);CHECK(config_flash_load(&d,&generation) && d.map[11]==12);
    c.deadzone[0]=12;fail_safe=1;CHECK(!config_flash_save(&c,&generation));fail_safe=0;CHECK(config_flash_load(&d,&generation) && d.deadzone[0]==0);
    CHECK(config_test_flash[0x3fd000]==255 && config_test_flash[0x3ff000]==255);
    bridge_t b;bridge_init(&b);bridge_usb_active(&b,true);bridge_connect(&b,PAD_XSX);bridge_report_sent(&b);
    config_defaults(&c);c.map[11]=12;c.map[12]=255;c.deadzone[0]=10;c.invert=4;c.rumble[0]=50;bridge_apply_config(&b,&c);
    pad_input_t in={.buttons=1,.axes={20,0,511,0}};bridge_input(&b,&in);CHECK(buttons(&b)==0x2000 && b.report[6]==0 && b.report[7]==0);
    CHECK((int16_t)(b.report[10]|b.report[11]<<8)==-32767);in.buttons=2;bridge_input(&b,&in);CHECK(!buttons(&b));
    uint8_t out[8]={0,8,0,200,100},strong,weak;bridge_take_rumble(&b,false,&strong,&weak);bridge_output(&b,out,8);CHECK(bridge_take_rumble(&b,false,&strong,&weak) && strong==100 && weak==100);
    config_defaults(&c);c.trigger=11;c.count=3;c.steps[0]=(config_step_t){0x2000,100,0,0};c.steps[1]=(config_step_t){0,100,100,0};c.steps[2]=(config_step_t){0x4000,100,0,0};
    bridge_apply_config(&b,&c);in.buttons=0;bridge_input(&b,&in);bridge_tick(&b,0xffffffc0u);in.buttons=1;bridge_input(&b,&in);CHECK(buttons(&b)==0x2000);
    bridge_tick(&b,36);CHECK(!buttons(&b) && b.report[4]==100);bridge_tick(&b,136);CHECK(buttons(&b)==0x4000);bridge_tick(&b,236);CHECK(!buttons(&b) && !b.macro_active);
    bridge_input(&b,&in);CHECK(!b.macro_active);in.buttons=0;bridge_input(&b,&in);in.buttons=1;bridge_input(&b,&in);CHECK(b.macro_active);
    bridge_usb_active(&b,false);CHECK(!b.macro_active && !buttons(&b));bridge_usb_active(&b,true);bridge_input(&b,&in);CHECK(!b.macro_active);
    in.buttons=0;bridge_input(&b,&in);in.buttons=1;bridge_input(&b,&in);bridge_disconnect(&b);CHECK(!b.macro_active && !buttons(&b));
    config_protocol_t p;config_protocol_init(&p,&b,2);command(&p,CFG_INFO,1,0,NULL,0);CHECK(p.response[9]==CFG_OK && p.response[12]==128);
    command(&p,CFG_BEGIN,11,0,NULL,0);CHECK(p.staged);command(&p,CFG_BEGIN,12,0,NULL,0);CHECK(p.response[9]==CFG_BUSY);
    command(&p,CFG_APPLY,11,0,NULL,0);CHECK(p.response[9]==CFG_INVALID);
    config_defaults(&c);c.map[11]=14;config_encode(&c,bytes);
    for(unsigned offset=0;offset<128;offset+=19) { unsigned n=128-offset<19?128-offset:19;command(&p,CFG_PUT,11,offset,bytes+offset,n);CHECK(p.response[9]==CFG_OK); }
    command(&p,CFG_APPLY,12,0,NULL,0);CHECK(p.response[9]==CFG_TOKEN);command(&p,CFG_APPLY,11,0,NULL,0);CHECK(p.response[9]==CFG_OK && b.config.map[11]==14 && !p.staged);
    command(&p,CFG_APPLY,11,0,NULL,0);CHECK(p.response[9]==CFG_OK); /* lost ACK retry */
    command(&p,CFG_SAVE,18,0,NULL,0);CHECK(!p.saving && p.response[9]==CFG_RANGE);
    uint8_t wrong[4]={0};command(&p,CFG_SAVE,19,0,wrong,4);CHECK(!p.saving && p.response[9]==CFG_INVALID);
    command(&p,CFG_SAVE,20,0,bytes+124,4);CHECK(p.saving && p.response[9]==CFG_BUSY);command(&p,CFG_DEFAULTS,21,0,NULL,0);CHECK(p.saving && p.response[5]==20);
    config_protocol_saved(&p,CFG_OK,3);CHECK(!p.saving && p.response[9]==CFG_OK);command(&p,CFG_SAVE,20,0,bytes+124,4);CHECK(!p.saving);
    command(&p,CFG_BEGIN,30,0,NULL,0);config_protocol_tick(&p,6000);command(&p,CFG_APPLY,30,0,NULL,0);CHECK(p.response[9]==CFG_NO_TRANSACTION);
    if(argc>1) {
        FILE *f=fopen(argv[1],"wb");CHECK(f!=NULL);config_defaults(&c);config_encode(&c,bytes);CHECK(fwrite(bytes,1,128,f)==128);
        c.map[11]=14;c.deadzone[0]=12;c.deadzone[1]=7;c.invert=5;c.rumble[0]=60;c.trigger=12;c.count=2;
        c.steps[0]=(config_step_t){0x1000,100,0,25};c.steps[1]=(config_step_t){0,80,10,0};config_encode(&c,bytes);CHECK(fwrite(bytes,1,128,f)==128);fclose(f);
    }
    printf("Config: %u checks passed\n",checks);return 0;
}
