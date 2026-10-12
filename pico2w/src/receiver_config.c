// SPDX-License-Identifier: Apache-2.0
#include "receiver_config.h"
#include <string.h>
const uint16_t config_button_bits[CONFIG_BUTTONS]={1,2,4,8,16,32,64,128,256,512,1024,4096,8192,16384,32768};
static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0]|p[1]<<8); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void put16(uint8_t *p,uint16_t v) { p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8); }
static void put32(uint8_t *p,uint32_t v) { for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i)); }
uint32_t config_crc32(const uint8_t *p,unsigned size) {
    uint32_t crc=0xffffffffu;
    for(unsigned i=0;i<size;i++) { crc^=p[i];for(unsigned j=0;j<8;j++)crc=(crc>>1)^((crc&1)?0xedb88320u:0); }
    return crc^0xffffffffu;
}
uint16_t config_crc16(const uint8_t *p,unsigned size) {
    uint16_t crc=0xffff;
    for(unsigned i=0;i<size;i++) { crc^=(uint16_t)p[i]<<8;for(unsigned j=0;j<8;j++)crc=(uint16_t)((crc<<1)^((crc&0x8000)?0x1021:0)); }
    return crc;
}
void config_defaults(receiver_config_t *c) {
    memset(c,0,sizeof(*c));c->rumble[0]=c->rumble[1]=100;c->trigger=255;
    for(unsigned i=0;i<CONFIG_BUTTONS;i++)c->map[i]=(uint8_t)i;
}
bool config_valid(const receiver_config_t *c) {
    if(c->invert>15 || c->deadzone[0]>50 || c->deadzone[1]>50 || c->rumble[0]>100 || c->rumble[1]>100 ||
        (c->trigger>=CONFIG_BUTTONS && c->trigger!=255) || c->count>CONFIG_STEPS || (c->trigger==255 && c->count))return false;
    for(unsigned i=0;i<CONFIG_BUTTONS;i++)if(c->map[i]>=CONFIG_BUTTONS && c->map[i]!=255)return false;
    uint32_t total=0;
    for(unsigned i=0;i<CONFIG_STEPS;i++) {
        const config_step_t *s=&c->steps[i];
        if(i<c->count) { if((s->buttons&0x0800) || s->duration<20 || s->duration>5000)return false;total+=s->duration; }
        else if(s->buttons || s->duration || s->lt || s->rt)return false;
    }
    return total<=30000;
}
void config_encode(const receiver_config_t *c,uint8_t out[CONFIG_SIZE]) {
    memset(out,0,CONFIG_SIZE);memcpy(out,"PCF1",4);out[4]=1;out[5]=c->invert;
    out[6]=c->deadzone[0];out[7]=c->deadzone[1];out[8]=c->rumble[0];out[9]=c->rumble[1];out[10]=c->trigger;out[11]=c->count;
    memcpy(out+12,c->map,CONFIG_BUTTONS);
    for(unsigned i=0;i<CONFIG_STEPS;i++) { uint8_t *p=out+28+6*i;put16(p,c->steps[i].buttons);put16(p+2,c->steps[i].duration);p[4]=c->steps[i].lt;p[5]=c->steps[i].rt; }
    put32(out+124,config_crc32(out,124));
}
bool config_decode(receiver_config_t *c,const uint8_t in[CONFIG_SIZE]) {
    if(memcmp(in,"PCF1",4) || in[4]!=1 || in[27] || get32(in+124)!=config_crc32(in,124))return false;
    receiver_config_t next;memset(&next,0,sizeof(next));next.invert=in[5];next.deadzone[0]=in[6];next.deadzone[1]=in[7];
    next.rumble[0]=in[8];next.rumble[1]=in[9];next.trigger=in[10];next.count=in[11];memcpy(next.map,in+12,CONFIG_BUTTONS);
    for(unsigned i=0;i<CONFIG_STEPS;i++) { const uint8_t *p=in+28+6*i;next.steps[i]=(config_step_t){get16(p),get16(p+2),p[4],p[5]}; }
    if(!config_valid(&next))return false;
    *c=next;return true;
}
void config_record_make(uint8_t page[256],const receiver_config_t *c,uint32_t generation) {
    memset(page,0xff,256);memcpy(page,"PCJ1",4);put32(page+4,generation);config_encode(c,page+8);
    put32(page+136,config_crc32(page,136));memcpy(page+140,"DONE",4);
}
bool config_record_read(const uint8_t page[256],receiver_config_t *c,uint32_t *generation) {
    if(memcmp(page,"PCJ1",4) || memcmp(page+140,"DONE",4) || get32(page+136)!=config_crc32(page,136) || !config_decode(c,page+8))return false;
    *generation=get32(page+4);return true;
}
