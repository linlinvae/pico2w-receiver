// SPDX-License-Identifier: Apache-2.0
#include "battery.h"
#include "ble/gatt-service/battery_service_client.h"
#include "hid_usage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned checks, connects, disconnects, reads, parses, switch_observations;
static uint8_t connect_result, read_result;
static uint16_t next_cid = 10;
static btstack_packet_handler_t handler;
void receiver_switch_report_received(uni_hid_device_t *d, const uint8_t *p, uint16_t n) {
    (void)d; (void)p; (void)n; ++switch_observations;
}
#define CHECK(x) do { assert(x); ++checks; } while (0)
void battery_service_client_init(void) {}
uint8_t battery_service_client_connect(hci_con_handle_t handle, btstack_packet_handler_t cb, uint32_t poll, uint16_t *cid) {
    CHECK(handle == 7 && poll == 0); handler = cb; *cid = next_cid++; ++connects; return connect_result;
}
uint8_t battery_service_client_disconnect(uint16_t cid) { CHECK(cid != 0); ++disconnects; return 0; }
uint8_t battery_service_client_read_battery_level(uint16_t cid, uint8_t index) {
    CHECK(cid != 0 && index == 0); ++reads; return read_result;
}
void __real_uni_hid_parser_switch_parse_input_report(uni_hid_device_t *d, const uint8_t *p, uint16_t n) {
    (void)d; (void)p; (void)n; ++parses;
}
void __wrap_uni_hid_parser_switch_parse_input_report(uni_hid_device_t *, const uint8_t *, uint16_t);
void __real_uni_hid_parser_xboxone_parse_usage(uni_hid_device_t *d, const hid_globals_t *g, uint16_t page, uint16_t usage, int32_t value) {
    (void)d; (void)g; (void)page; (void)usage; (void)value;
}
void __wrap_uni_hid_parser_xboxone_parse_usage(uni_hid_device_t *, const hid_globals_t *, uint16_t, uint16_t, int32_t);
static void emit(uint8_t sub, uint16_t cid, uint8_t status, uint8_t value) {
    uint8_t p[8] = {HCI_EVENT_GATTSERVICE_META,6,sub,(uint8_t)cid,(uint8_t)(cid>>8),0,0,value};
    p[sub == GATTSERVICE_SUBEVENT_BATTERY_SERVICE_CONNECTED ? 5 : 6] = status;
    handler(HCI_EVENT_PACKET,0,p,8);
}
static void status(uint8_t *p) { receiver_battery_status(p); CHECK(!memcmp(p,"P2W",3) && p[3]==1 && p[11]==2); }
int main(void) {
    uni_hid_device_t d = {0}, other = {0}; d.conn.handle=7;
    uint8_t p[31]; receiver_battery_init(); receiver_battery_detach(); status(p);
    CHECK(!p[4] && !p[5] && p[7]==255 && p[8]==255);
    receiver_battery_attach(&d,PAD_STADIA,100); receiver_battery_service(1099); CHECK(connects==0);
    receiver_battery_service(1100); CHECK(connects==1);
    emit(GATTSERVICE_SUBEVENT_BATTERY_SERVICE_CONNECTED,10,0,0);
    receiver_battery_service(1101); CHECK(reads==1);
    emit(GATTSERVICE_SUBEVENT_BATTERY_SERVICE_LEVEL,10,0,0); status(p);
    CHECK(p[5] && p[7]==0 && p[6]==1); /* Zero percent is valid. */
    emit(GATTSERVICE_SUBEVENT_BATTERY_SERVICE_LEVEL,10,0,100); status(p); CHECK(p[7]==100);
    emit(GATTSERVICE_SUBEVENT_BATTERY_SERVICE_LEVEL,10,0,255); status(p); CHECK(p[7]==100 && p[10]==3);
    emit(GATTSERVICE_SUBEVENT_BATTERY_SERVICE_LEVEL,99,0,20); status(p); CHECK(p[7]==100);
    receiver_battery_service(31100); CHECK(reads==1);
    receiver_battery_service(31101); CHECK(reads==2);
    receiver_battery_service(121102); status(p); CHECK(!p[5] && p[7]==255);
    receiver_battery_detach(); CHECK(disconnects==1);
    emit(GATTSERVICE_SUBEVENT_BATTERY_SERVICE_LEVEL,10,0,80); status(p); CHECK(!p[4] && !p[5]);
    receiver_battery_attach(&d,PAD_XSX,130000); receiver_battery_service(131000);
    emit(GATTSERVICE_SUBEVENT_BATTERY_SERVICE_LEVEL,10,0,80); status(p); CHECK(!p[5]);
    emit(GATTSERVICE_SUBEVENT_BATTERY_SERVICE_CONNECTED,11,0x11,0);
    receiver_battery_detach(); CHECK(disconnects==1); /* Failed connection is freed by BTstack. */
    connect_result=0x0c; receiver_battery_attach(&d,PAD_XSX,140000); receiver_battery_service(141000);
    status(p); CHECK(p[10]==2 && !p[5] && disconnects==2);
    receiver_battery_detach(); CHECK(disconnects==2);
    receiver_battery_attach(&d,PAD_XSX,145000);
    hid_globals_t g={.logical_minimum=1,.logical_maximum=255};
    __wrap_uni_hid_parser_xboxone_parse_usage(&d,&g,HID_USAGE_PAGE_GENERIC_DEVICE_CONTROLS,HID_USAGE_BATTERY_STRENGTH,0);
    status(p); CHECK(!p[5]);
    __wrap_uni_hid_parser_xboxone_parse_usage(&d,&g,HID_USAGE_PAGE_GENERIC_DEVICE_CONTROLS,HID_USAGE_BATTERY_STRENGTH,128);
    status(p); CHECK(p[5] && p[6]==3 && p[7]==255 && p[16]==128 && p[17]==1 && p[18]==255);
    __wrap_uni_hid_parser_xboxone_parse_usage(&d,&g,HID_USAGE_PAGE_GENERIC_DEVICE_CONTROLS,HID_USAGE_BATTERY_STRENGTH,256);
    status(p); CHECK(p[16]==128);
    receiver_battery_attach(&d,PAD_SWITCH_PRO,150000);
    uint8_t report[12]={0x30,0,0x90};
    __wrap_uni_hid_parser_switch_parse_input_report(&other,report,12); status(p); CHECK(!p[5]);
    __wrap_uni_hid_parser_switch_parse_input_report(&d,report,2); status(p); CHECK(!p[5]);
    for(unsigned i=0;i<=4;i++) {
        report[2]=(uint8_t)((i<<5)|0x10);
        __wrap_uni_hid_parser_switch_parse_input_report(&d,report,12); status(p);
        CHECK(p[5] && p[6]==2 && p[7]==255 && p[8]==i && p[9]==1);
    }
    report[2]=0xe0; __wrap_uni_hid_parser_switch_parse_input_report(&d,report,12); status(p); CHECK(p[8]==4);
    CHECK(parses==8 && switch_observations==8); receiver_battery_detach(); status(p); CHECK(!p[4] && !p[5] && p[8]==255);
    printf("Battery: %u checks passed\n",checks); return 0;
}
