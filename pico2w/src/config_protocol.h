// SPDX-License-Identifier: Apache-2.0
#ifndef CONFIG_PROTOCOL_H
#define CONFIG_PROTOCOL_H
#include "bridge.h"
#define CONFIG_PACKET_SIZE 31
enum { CFG_INFO=1,CFG_READ,CFG_BEGIN,CFG_PUT,CFG_APPLY,CFG_SAVE,CFG_DEFAULTS,CFG_CANCEL };
enum { CFG_OK,CFG_BUSY,CFG_BAD_PACKET,CFG_RANGE,CFG_INVALID,CFG_IO,CFG_UNSUPPORTED,CFG_NO_TRANSACTION,CFG_TOKEN,CFG_NEEDS_IDLE };
typedef struct {
    bridge_t *bridge;
    uint8_t staging[CONFIG_SIZE],covered[CONFIG_SIZE],response[31],last[31];
    bool staged,saving,have_last,rebooting;
    uint16_t token;
    uint32_t now,updated,generation;
} config_protocol_t;
void config_protocol_init(config_protocol_t *p,bridge_t *b,uint32_t generation);
void config_protocol_set(config_protocol_t *p,const uint8_t *packet,unsigned length);
void config_protocol_tick(config_protocol_t *p,uint32_t now);
void config_protocol_saved(config_protocol_t *p,uint8_t status,uint32_t generation);
#endif
