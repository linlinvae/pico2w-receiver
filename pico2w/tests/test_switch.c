// SPDX-License-Identifier: Apache-2.0
/* Exercise the real TinyUSB HID driver and shared bridge, without USB hardware. */
#include "bridge.h"
#include "switch_report.h"
#include "usb_switch.h"
#include "usb_switch_descriptors.h"
#include "tusb.h"
#include "device/usbd_pvt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned checks, sent_count, out_count;
static bool mounted, suspended, busy, fail_xfer;
static uint8_t sent[8], ctrl[128], *rx, *inflight;
static void *control_out;
static uint16_t control_len;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); assert(x); } } while (0)
bool tusb_rhport_init(uint8_t port, const tusb_rhport_init_t *init) { (void)port; (void)init; return true; }
void tud_task_ext(uint32_t timeout, bool isr) { (void)timeout; CHECK(!isr); }
bool tud_mounted(void) { return mounted; }
bool tud_suspended(void) { return suspended; }
bool usbd_edpt_open(uint8_t port, const tusb_desc_endpoint_t *ep) {
    CHECK(!port && ep->bmAttributes.xfer == TUSB_XFER_INTERRUPT);
    CHECK(ep->wMaxPacketSize == 64 && ep->bInterval == 1); return true;
}
bool usbd_open_edpt_pair(uint8_t port, const uint8_t *desc, uint8_t n,
        uint8_t type, uint8_t *out, uint8_t *in) {
    CHECK(n == 2 && type == TUSB_XFER_INTERRUPT);
    CHECK(desc[2] == SWITCH_EP_OUT && desc[9] == SWITCH_EP_IN);
    *out = desc[2]; *in = desc[9];
    return usbd_edpt_open(port,(const tusb_desc_endpoint_t *)desc) &&
        usbd_edpt_open(port,(const tusb_desc_endpoint_t *)(desc + 7));
}
bool usbd_edpt_busy(uint8_t port, uint8_t ep) { CHECK(!port && ep == SWITCH_EP_IN); return busy; }
bool usbd_edpt_claim(uint8_t port, uint8_t ep) {
    CHECK(!port && ep == SWITCH_EP_IN); if (busy) return false; busy = true; return true;
}
bool usbd_edpt_release(uint8_t port, uint8_t ep) { CHECK(!port && ep == SWITCH_EP_IN); busy=false; return true; }
bool usbd_edpt_xfer(uint8_t port, uint8_t ep, uint8_t *buffer, uint16_t size) {
    CHECK(!port);
    if (ep == SWITCH_EP_OUT) { CHECK(size == 64); rx=buffer; ++out_count; return true; }
    CHECK(ep == SWITCH_EP_IN && size == 8 && busy);
    /* Match usbd.c: failed DCD submission releases busy/claimed state. */
    if (fail_xfer) { busy=false; return false; }
    inflight=buffer; memcpy(sent,buffer,8); ++sent_count; return true;
}
bool tud_control_xfer(uint8_t port, const tusb_control_request_t *r, void *buffer, uint16_t size) {
    CHECK(!port && size <= sizeof(ctrl)); control_len=size;
    if (r->bmRequestType & 0x80) memcpy(ctrl,buffer,size); else control_out=buffer;
    return true;
}
bool tud_control_status(uint8_t port, const tusb_control_request_t *r) {
    CHECK(!port && !r->wLength); control_len=0; return true;
}
static void complete(const usbd_class_driver_t *d, xfer_result_t result) {
    busy=false; CHECK(d->xfer_cb(0,SWITCH_EP_IN,result,8));
}
static void neutral(const uint8_t *r) {
    static const uint8_t expected[8]={0,0,8,128,128,128,128,0};
    CHECK(!memcmp(r,expected,8));
}
/* Validate byte length, main-item usages, no report IDs, and 64-bit IN/OUT. */
static void descriptor_check(void) {
    const uint8_t *p=tud_hid_descriptor_report_cb(0);
    unsigned size=0,count=0,input=0,output=0; bool usage=false;
    for (unsigned i=0;i<SWITCH_HID_DESCRIPTOR_SIZE;) {
        uint8_t prefix=p[i++]; CHECK(prefix != 0xfe);
        unsigned len=prefix&3; if (len==3) len=4;
        CHECK(i+len <= SWITCH_HID_DESCRIPTOR_SIZE);
        unsigned type=(prefix>>2)&3, tag=prefix>>4;
        uint32_t value=0; for (unsigned j=0;j<len;j++) value |= (uint32_t)p[i+j] << (8*j);
        if (type==1 && tag==7) size=value;
        if (type==1 && tag==9) count=value;
        CHECK(!(type==1 && tag==8)); /* Report ID would change wire format. */
        if (type==2 && (tag==0 || tag==1)) usage=true;
        if (type==0) {
            if (tag==8 || tag==9) { CHECK((value&1) || usage); if (tag==8) input+=size*count; else output+=size*count; }
            usage=false;
        }
        i+=len;
    }
    CHECK(input==64 && output==64);
}
static void mapping_checks(void) {
    for (pad_kind_t kind=PAD_XSX;kind<=PAD_STADIA;kind++) {
        bridge_t b; bridge_init(&b); bridge_connect(&b,kind); bridge_usb_active(&b,true);
        uint8_t r[8]; switch_report(&b,r); neutral(r); bridge_report_sent(&b);
        pad_input_t in={0};
        static const uint16_t expected[]={2,4,1,8,16,32,64,128,1024,2048};
        for (unsigned i=0;i<10;i++) {
            in.buttons=(uint16_t)(1u<<i); in.brake=in.throttle=0;
            if (kind != PAD_SWITCH_PRO && i==6) in.brake=1023;
            if (kind != PAD_SWITCH_PRO && i==7) in.throttle=1023;
            bridge_input(&b,&in); switch_report(&b,r);
            CHECK((uint16_t)(r[0]|r[1]<<8)==expected[i]);
        }
        in=(pad_input_t){0};
        static const uint16_t misc[]={4096,256,512,8192};
        for (unsigned i=0;i<4;i++) {
            in.misc=(uint8_t)(1u<<i); bridge_input(&b,&in); switch_report(&b,r);
            CHECK((uint16_t)(r[0]|r[1]<<8)==misc[i]);
        }
        in=(pad_input_t){0};
        /* Bluepad32 up/down/right/left bits -> all eight Switch hat values. */
        static const uint8_t hats[16]={8,0,4,8,2,1,3,2,6,7,5,6,8,0,4,8};
        for (unsigned i=0;i<16;i++) { in.dpad=(uint8_t)i; bridge_input(&b,&in); switch_report(&b,r); CHECK(r[2]==hats[i]); }
        in=(pad_input_t){.axes={-512,-512,511,511}};
        bridge_input(&b,&in); switch_report(&b,r); CHECK(!r[3] && !r[4] && r[5]==255 && r[6]==255);
        in.axes[0]=in.axes[1]=511; in.axes[2]=in.axes[3]=-512;
        bridge_input(&b,&in); switch_report(&b,r); CHECK(r[3]==255 && r[4]==255 && !r[5] && !r[6]);
        in=(pad_input_t){0}; bridge_input(&b,&in); switch_report(&b,r); neutral(r);
        unsigned last=0;
        for (int axis=-700;axis<=700;axis++) {
            in.axes[0]=in.axes[1]=axis; bridge_input(&b,&in); switch_report(&b,r);
            CHECK(r[3]>=last); last=r[3]; CHECK(r[3]==r[4]);
        }
        if (kind!=PAD_SWITCH_PRO) {
            in=(pad_input_t){.buttons=0xc0,.brake=511,.throttle=511};
            bridge_input(&b,&in); switch_report(&b,r); CHECK(!(r[0]&0xc0));
            in.brake=in.throttle=512; bridge_input(&b,&in); switch_report(&b,r); CHECK((r[0]&0xc0)==0xc0);
        }
        /* Disconnect and USB suspend must release Capture as well as common input. */
        in=(pad_input_t){.buttons=1,.misc=8}; bridge_input(&b,&in);
        bridge_usb_active(&b,false); switch_report(&b,r); neutral(r);
        bridge_usb_active(&b,true); switch_report(&b,r); neutral(r); bridge_report_sent(&b);
        switch_report(&b,r); CHECK(r[0]==2 && r[1]==32);
        bridge_disconnect(&b); switch_report(&b,r); neutral(r);
    }
    /* The existing configuration must still apply before Switch conversion. */
    bridge_t b; bridge_init(&b); bridge_connect(&b,PAD_XSX); bridge_usb_active(&b,true);
    receiver_config_t c=b.config; c.map[11]=12; c.deadzone[0]=20; c.invert=1;
    bridge_apply_config(&b,&c); bridge_report_sent(&b);
    pad_input_t in={.buttons=1,.axes={50,50,0,0}}; uint8_t r[8];
    bridge_input(&b,&in); switch_report(&b,r); CHECK(r[0]==4 && r[3]==128 && r[4]==128);
    in.axes[0]=511; bridge_input(&b,&in); switch_report(&b,r); CHECK(!r[3]);
    config_defaults(&c); c.trigger=11; c.count=1; c.steps[0]=(config_step_t){.buttons=0x2000,.duration=20,.lt=255};
    bridge_apply_config(&b,&c); bridge_report_sent(&b); in=(pad_input_t){0}; bridge_input(&b,&in);
    in.buttons=1; bridge_input(&b,&in); switch_report(&b,r); CHECK(r[0]==0x44);
    bridge_tick(&b,20); switch_report(&b,r); CHECK(!r[0] && !r[1]);
}
int main(void) {
    descriptor_check(); mapping_checks();
    const tusb_desc_device_t *dev=(const tusb_desc_device_t *)tud_descriptor_device_cb();
    CHECK(dev->idVendor==0x0f0d && dev->idProduct==0x0092 && !dev->iSerialNumber);
    CHECK(switch_configuration[2]==41 && switch_configuration[4]==1 && switch_configuration[8]==125);
    CHECK(tud_descriptor_configuration_cb(1)==NULL && tud_hid_descriptor_report_cb(1)==NULL);
    CHECK(tud_descriptor_string_cb(0,0)[1]==0x0409 && tud_descriptor_string_cb(2,0)!=NULL);
    CHECK(tud_descriptor_string_cb(0xee,0)==NULL && tud_descriptor_string_cb(3,0)==NULL);
    bridge_t b; bridge_init(&b); usb_switch_init(&b);
    uint8_t count; const usbd_class_driver_t *d=usbd_app_driver_get_cb(&count); CHECK(count==1); d->init();
    CHECK(d->open(0,(const tusb_desc_interface_t *)(switch_configuration+9),32)==32);
    CHECK(rx && out_count==1);
    mounted=true; tud_mount_cb(); bridge_connect(&b,PAD_STADIA);
    pad_input_t in={.buttons=1,.misc=8,.throttle=1023}; bridge_input(&b,&in);
    usb_switch_service(1); CHECK(sent_count==1); neutral(sent);
    complete(d,XFER_RESULT_SUCCESS); usb_switch_service(2); CHECK(sent_count==2 && sent[0]==0x82 && sent[1]==32);
    uint8_t frozen[8]; memcpy(frozen,inflight,8);
    in.buttons=2; in.misc=0; bridge_input(&b,&in); usb_switch_service(3);
    CHECK(sent_count==2 && !memcmp(inflight,frozen,8));
    bridge_disconnect(&b); usb_switch_service(4); CHECK(sent_count==2);
    complete(d,XFER_RESULT_SUCCESS); usb_switch_service(4); CHECK(sent_count==3); neutral(sent);
    complete(d,XFER_RESULT_SUCCESS);
    bridge_connect(&b,PAD_SWITCH_PRO); in=(pad_input_t){.buttons=1}; bridge_input(&b,&in);
    fail_xfer=true; usb_switch_service(5); CHECK(b.force_neutral && !busy && sent_count==3);
    fail_xfer=false; usb_switch_service(5); CHECK(!b.force_neutral && sent_count==4); neutral(sent);
    complete(d,XFER_RESULT_SUCCESS); usb_switch_service(5); CHECK(sent_count==4);
    usb_switch_service(6); CHECK(sent_count==5 && sent[0]==2);
    complete(d,XFER_RESULT_FAILED); CHECK(b.force_neutral);
    usb_switch_service(6); CHECK(sent_count==6); neutral(sent); complete(d,XFER_RESULT_SUCCESS);
    /* GET_REPORT and descriptor requests go through actual TinyUSB parsing. */
    tusb_control_request_t req={.bmRequestType=0xa1,.bRequest=1,.wValue=0x0100,.wLength=8};
    CHECK(d->control_xfer_cb(0,CONTROL_STAGE_SETUP,&req)); CHECK(control_len==8 && ctrl[0]==2);
    req.wValue=0x0101; CHECK(!d->control_xfer_cb(0,CONTROL_STAGE_SETUP,&req));
    req.wValue=0x0300; CHECK(!d->control_xfer_cb(0,CONTROL_STAGE_SETUP,&req));
    req.wValue=0x0100; req.wIndex=0x0100; CHECK(!d->control_xfer_cb(0,CONTROL_STAGE_SETUP,&req));
    req.wIndex=0;
    req.wLength=3; CHECK(d->control_xfer_cb(0,CONTROL_STAGE_SETUP,&req)); CHECK(control_len==3);
    uint8_t guarded[10]; memset(guarded,0xaa,10);
    CHECK(tud_hid_get_report_cb(0,0,HID_REPORT_TYPE_INPUT,guarded+1,3)==3 && guarded[0]==0xaa && guarded[4]==0xaa);
    CHECK(!tud_hid_get_report_cb(1,0,HID_REPORT_TYPE_INPUT,guarded,8));
    CHECK(!tud_hid_get_report_cb(0,0,HID_REPORT_TYPE_FEATURE,guarded,8));
    CHECK(!tud_hid_get_report_cb(0,1,HID_REPORT_TYPE_INPUT,guarded,8));
    req=(tusb_control_request_t){.bmRequestType=0x81,.bRequest=6,.wValue=0x2200,.wLength=86};
    CHECK(d->control_xfer_cb(0,CONTROL_STAGE_SETUP,&req)); CHECK(control_len==86 && !memcmp(ctrl,tud_hid_descriptor_report_cb(0),86));
    req.wValue=0x2100; CHECK(d->control_xfer_cb(0,CONTROL_STAGE_SETUP,&req)); CHECK(control_len==9 && ctrl[7]==86);
    req=(tusb_control_request_t){.bmRequestType=0x21,.bRequest=10};
    CHECK(d->control_xfer_cb(0,CONTROL_STAGE_SETUP,&req)); CHECK(control_len==0);
    /* OUT (interrupt and control) is accepted and rearmed, never triggers rumble. */
    uint8_t strong,weak; bridge_take_rumble(&b,false,&strong,&weak);
    memcpy(rx,(uint8_t[]){0,8,0,255,255,0,0,0},8);
    CHECK(d->xfer_cb(0,SWITCH_EP_OUT,XFER_RESULT_SUCCESS,8)); CHECK(out_count==2 && !b.strong && !b.weak);
    CHECK(d->xfer_cb(0,SWITCH_EP_OUT,XFER_RESULT_FAILED,8)); CHECK(out_count==3);
    req=(tusb_control_request_t){.bmRequestType=0x21,.bRequest=9,.wValue=0x0200,.wLength=8};
    CHECK(d->control_xfer_cb(0,CONTROL_STAGE_SETUP,&req)); memcpy(control_out,rx,8);
    CHECK(d->control_xfer_cb(0,CONTROL_STAGE_ACK,&req)); CHECK(!b.strong && !b.weak && !usb_switch_config()->staged);
    req.wLength=7; CHECK(!d->control_xfer_cb(0,CONTROL_STAGE_SETUP,&req));
    req.wLength=8; req.wValue=0x0302; CHECK(!d->control_xfer_cb(0,CONTROL_STAGE_SETUP,&req));
    suspended=true; tud_suspend_cb(false); CHECK(!b.usb_active);
    unsigned before=sent_count; usb_switch_service(10); CHECK(before==sent_count);
    suspended=false; tud_resume_cb(); CHECK(b.force_neutral); usb_switch_service(10); neutral(sent); complete(d,XFER_RESULT_SUCCESS);
    d->reset(0); CHECK(!b.usb_active); usb_switch_service(11); CHECK(before+1==sent_count);
    /* No mount => no report after re-enumeration. */
    CHECK(d->open(0,(const tusb_desc_interface_t *)(switch_configuration+9),32)==32);
    usb_switch_service(12); CHECK(before+1==sent_count);
    tud_mount_cb(); usb_switch_service(12); neutral(sent); complete(d,XFER_RESULT_SUCCESS);
    tud_umount_cb(); CHECK(!b.usb_active);
    printf("Switch HID and mapping: %u checks passed\n",checks); return 0;
}
