// SPDX-License-Identifier: Apache-2.0
#include "config_protocol.h"
#include <string.h>
static void crc(uint8_t *r) { uint16_t v=config_crc16(r,29);r[29]=(uint8_t)v;r[30]=(uint8_t)(v>>8); }
static void info(config_protocol_t *p) {
    uint8_t *r=p->response;r[8]=10;r[10]=1;r[11]=31;r[12]=CONFIG_SIZE;r[13]=CONFIG_STEPS;
    for(unsigned i=0;i<4;i++)r[14+i]=(uint8_t)(p->generation>>(i*8));
    r[18]=p->staged;r[19]=p->saving;
}
void config_protocol_init(config_protocol_t *p,bridge_t *b,uint32_t generation) {
    memset(p,0,sizeof(*p));p->bridge=b;p->generation=generation;
    memcpy(p->response,"P2C",3);p->response[3]=1;p->response[4]=CFG_INFO;info(p);crc(p->response);
}
void config_protocol_tick(config_protocol_t *p,uint32_t now) {
    p->now=now;if(p->staged && (uint32_t)(now-p->updated)>5000) { p->staged=false;p->have_last=false; }
}
void config_protocol_saved(config_protocol_t *p,uint8_t status,uint32_t generation) {
    p->saving=false;p->generation=generation;p->response[9]=status;info(p);crc(p->response);
}
void config_protocol_set(config_protocol_t *p,const uint8_t *q,unsigned length) {
    if(!q || length!=31)return;
    if(p->have_last && !memcmp(p->last,q,31))return; /* Retried writes are idempotent. */
    if(p->saving || p->rebooting)return; /* Keep pending SAVE response/token stable. */
    uint8_t *r=p->response;memset(r,0,31);memcpy(r,"P2C",3);r[3]=1;r[4]=q[4];r[5]=q[5];r[6]=q[6];r[7]=q[7];
    uint16_t token=(uint16_t)(q[5]|q[6]<<8);
    if(memcmp(q,"P2C",3) || q[3]!=1 || q[8]>19 || q[9] || config_crc16(q,29)!=(uint16_t)(q[29]|q[30]<<8)) { r[9]=CFG_BAD_PACKET;crc(r);return; }
    memcpy(p->last,q,31);p->have_last=true;
    if(q[4]==CFG_INFO)info(p);
    else if(q[4]==CFG_READ) {
        if(!q[8] || q[7]+q[8]>CONFIG_SIZE)r[9]=CFG_RANGE;
        else { uint8_t data[CONFIG_SIZE];config_encode(&p->bridge->config,data);r[8]=q[8];memcpy(r+10,data+q[7],q[8]); }
    } else if(q[4]==CFG_BEGIN) {
        if(!token)r[9]=CFG_TOKEN;
        else if(p->staged && p->token!=token)r[9]=CFG_BUSY;
        else { memset(p->covered,0,sizeof(p->covered));p->staged=true;p->token=token;p->updated=p->now; }
    } else if(q[4]==CFG_PUT || q[4]==CFG_APPLY || q[4]==CFG_CANCEL) {
        if(!p->staged)r[9]=CFG_NO_TRANSACTION;
        else if(p->token!=token)r[9]=CFG_TOKEN;
        else {
            p->updated=p->now;
            if(q[4]==CFG_CANCEL)p->staged=false;
            else if(q[4]==CFG_PUT) {
                if(!q[8] || q[7]+q[8]>CONFIG_SIZE)r[9]=CFG_RANGE;
                else { memcpy(p->staging+q[7],q+10,q[8]);memset(p->covered+q[7],1,q[8]); }
            } else {
                for(unsigned i=0;i<CONFIG_SIZE;i++)if(!p->covered[i])r[9]=CFG_INVALID;
                receiver_config_t next;
                if(!r[9] && !config_decode(&next,p->staging))r[9]=CFG_INVALID;
                if(!r[9]) { bridge_apply_config(p->bridge,&next);p->staged=false; }
            }
        }
    } else if(q[4]==CFG_SAVE) {
        if(p->staged)r[9]=CFG_BUSY;
        else if(q[8]!=4)r[9]=CFG_RANGE;
        else {
            uint8_t data[CONFIG_SIZE];config_encode(&p->bridge->config,data);
            /* Bind persistence to the configuration the host read back. */
            if(memcmp(data+124,q+10,4))r[9]=CFG_INVALID;
            else { p->saving=true;r[9]=CFG_BUSY; }
        }
    } else if(q[4]==CFG_DEFAULTS) {
        if(p->staged)r[9]=CFG_BUSY;
        else { receiver_config_t next;config_defaults(&next);bridge_apply_config(p->bridge,&next); }
    } else r[9]=CFG_UNSUPPORTED;
    crc(r);
}
