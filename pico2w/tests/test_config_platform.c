// SPDX-License-Identifier: Apache-2.0
#define main legacy_platform_test_main
#include "test_platform.c"
#undef main
static void save_request(void) {
    uint8_t q[31]={'P','2','C',1,CFG_SAVE,42};q[8]=4;
    uint8_t encoded[128];config_encode(&usb_bridge->config,encoded);memcpy(q+10,encoded+124,4);
    uint16_t crc=config_crc16(q,29);q[29]=(uint8_t)crc;q[30]=(uint8_t)(crc>>8);
    config_protocol_set(&config_proto,q,31);
}
int main(int argc,char **argv) {
    CHECK(argc==2);
    platform=receiver_platform();platform->init(0,NULL);platform->on_init_complete();advance(101);
    if(!strcmp(argv[1],"connected") || !strcmp(argv[1],"pairing")) {
        prepare(PAD_XSX);
        if(!strcmp(argv[1],"connected"))CHECK(platform->on_device_ready(&device)==UNI_ERROR_SUCCESS);
        save_request();advance(2);
        CHECK(config_proto.response[9]==CFG_NEEDS_IDLE && !config_saves && !reboots);
        CHECK(radio_state==HCI_STATE_WORKING);
    } else {
        save_failure=!strcmp(argv[1],"failure");block_power_off=!strcmp(argv[1],"timeout");
        save_request();CHECK(config_proto.saving && config_proto.response[9]==CFG_BUSY);advance(2);
        if(block_power_off) {CHECK(config_proto.saving && !config_saves && !reboots);advance(2001);}
        CHECK(!config_proto.saving && config_proto.rebooting && reboots==1);
        CHECK(config_proto.response[9]==((save_failure||block_power_off)?CFG_IO:CFG_OK));
        CHECK(config_proto.response[5]==42 && config_proto.response[4]==CFG_SAVE);
        CHECK(config_saves==(block_power_off?0u:1u));
        CHECK(!incoming && !ble_scan && !key_deletes && !bond_clears);
        unsigned old_scan=scan_calls;advance(200);CHECK(scan_calls==old_scan && reboots==1);
        prepare(PAD_XSX);CHECK(platform->on_device_ready(&device)!=UNI_ERROR_SUCCESS);
        CHECK(config_proto.generation==((save_failure||block_power_off)?0u:1u));
    }
    printf("Config platform (%s): %u checks passed\n",argv[1],checks);return 0;
}
