#include "bridge.h"
#include "usb_xinput.h"
#include "usb_descriptors.h"
#include "tusb.h"
#include "device/usbd_pvt.h"
#include "pico/unique_id.h"
#include "class/hid/hid_device.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned checks, input_count, output_count, control_count;
static uint8_t *rx, *inflight;
static uint8_t sent[20], ctrl[64];
static void *control_out;
static uint16_t control_len;
static bool busy, mounted, suspended, fail_xfer;
#define CHECK(x) do { assert(x); ++checks; } while (0)
bool tusb_rhport_init(uint8_t port, const tusb_rhport_init_t *init) { (void)port; (void)init; return true; }
void tud_task_ext(uint32_t timeout, bool isr) { (void)timeout; CHECK(!isr); }
bool tud_mounted(void) { return mounted; }
bool tud_suspended(void) { return suspended; }
bool usbd_edpt_open(uint8_t port, const tusb_desc_endpoint_t *ep) {
    CHECK(port == 0); CHECK(ep->bmAttributes.xfer == TUSB_XFER_INTERRUPT);
    CHECK(ep->wMaxPacketSize == 32); return true;
}
bool usbd_edpt_busy(uint8_t port, uint8_t ep) { (void)port; (void)ep; return false; }
bool usbd_open_edpt_pair(uint8_t port, const uint8_t *desc, uint8_t n, uint8_t type, uint8_t *out, uint8_t *in) {
    CHECK(n==1 && type==TUSB_XFER_INTERRUPT); *out=0; *in=desc[2];
    CHECK(*in==0x81); return usbd_edpt_open(port,(const tusb_desc_endpoint_t *)desc);
}
bool usbd_edpt_claim(uint8_t port, uint8_t ep) { CHECK(!port && ep==0x83); if (busy) return false; busy=true; return true; }
bool usbd_edpt_release(uint8_t port, uint8_t ep) { CHECK(!port && ep==0x83); busy=false; return true; }
bool usbd_edpt_xfer(uint8_t port, uint8_t ep, uint8_t *buffer, uint16_t size) {
    CHECK(port == 0);
    if (ep==0x03) { CHECK(size == 32); rx=buffer; ++output_count; return true; }
    CHECK(ep==0x83 && size==20 && busy);
    if (fail_xfer) return false;
    inflight=buffer; memcpy(sent,buffer,size); ++input_count; return true;
}
bool tud_control_xfer(uint8_t port, const tusb_control_request_t *r, void *buffer, uint16_t size) {
    CHECK(port == 0); ++control_count; control_len=size;
    if (r->bmRequestType & 0x80) { CHECK(size <= sizeof(ctrl)); memcpy(ctrl,buffer,size); }
    else control_out=buffer;
    return true;
}
bool tud_control_status(uint8_t port, const tusb_control_request_t *r) {
    CHECK(!port && !r->wLength); ++control_count; control_len=0; return true;
}
void pico_get_unique_board_id_string(char *id, size_t len) { snprintf(id,len,"0123456789ABCDEF"); }
void pico_get_unique_board_id(pico_unique_board_id_t *id) {
    static const uint8_t value[8]={0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef};
    memcpy(id->id,value,8);
}
static void complete(const usbd_class_driver_t *d) { busy=false; d->xfer_cb(0,0x83,XFER_RESULT_SUCCESS,20); }
void receiver_battery_status(uint8_t out[31]) { memset(out, 0, 31); memcpy(out, "P2W", 3); out[3]=1; out[11]=2; }
/* HID local items expire after every Main item, including Collection.
 * TinyUSB serves bytes without enforcing this host-side parser requirement. */
static bool main_items_have_usage(const uint8_t *descriptor, unsigned length) {
    bool usage = false;
    for (unsigned i=0; i<length;) {
        uint8_t prefix=descriptor[i++];
        if (prefix == 0xfe) return false;
        unsigned size=prefix & 3; if (size==3) size=4;
        if (size > length-i) return false;
        unsigned type=(prefix >> 2)&3, tag=prefix >> 4;
        if (type==2 && (tag==0 || tag==1)) usage=true;
        if (type==0) {
            if ((tag==8 || tag==9 || tag==11) && (!size || (!(descriptor[i]&1) && !usage))) return false;
            usage=false;
        }
        i+=size;
    }
    return true;
}
int main(void) {
    bridge_t b; bridge_init(&b); usb_xinput_init(&b);
    uint8_t count=0; const usbd_class_driver_t *d=usbd_app_driver_get_cb(&count);
    CHECK(count==1); d->init();
    CHECK(d->open(0,(const tusb_desc_interface_t *)(xinput_configuration+9),39)==0);
    uint8_t bad[40]; memcpy(bad,xinput_configuration+9,40); bad[7]=0;
    CHECK(d->open(0,(const tusb_desc_interface_t *)bad,40)==0);
    CHECK(d->open(0,(const tusb_desc_interface_t *)(xinput_configuration+9),40)==40);
    CHECK(output_count==1 && rx!=NULL);
    CHECK(xinput_configuration[2]==74 && xinput_configuration[4]==2 && xinput_configuration[8]==125);
    CHECK(xinput_configuration[41]==2 && xinput_configuration[48]==8);
    CHECK(xinput_configuration[51]==1 && xinput_configuration[54]==3 && xinput_configuration[71]==32);
    const uint8_t *hid=tud_hid_descriptor_report_cb(0);
    CHECK(hid && hid[0]==6 && hid[17]==31 && hid[38]==0xc0);
    CHECK(xinput_configuration[65]==STATUS_HID_DESCRIPTOR_SIZE);
    CHECK(main_items_have_usage(hid,STATUS_HID_DESCRIPTOR_SIZE));
    /* Reconstruct the R4 descriptor by removing Input's new local Usage. */
    uint8_t old_hid[25]; memcpy(old_hid,hid,22); memcpy(old_hid+22,hid+24,2);old_hid[24]=0xc0;
    CHECK(!main_items_have_usage(old_hid,sizeof(old_hid)));
    old_hid[23]=1; /* Constant padding does not require Usage. */
    CHECK(main_items_have_usage(old_hid,sizeof(old_hid)));
    CHECK(!main_items_have_usage(hid,STATUS_HID_DESCRIPTOR_SIZE-2));
    uint8_t feature[31];
    CHECK(tud_hid_get_report_cb(0,1,HID_REPORT_TYPE_FEATURE,feature,31)==31);
    CHECK(!memcmp(feature,"P2W",3) && feature[11]==2);
    CHECK(tud_hid_get_report_cb(0,1,HID_REPORT_TYPE_FEATURE,feature,30)==0);
    CHECK(tud_hid_get_report_cb(0,1,HID_REPORT_TYPE_INPUT,feature,31)==0);
    CHECK(tud_hid_get_report_cb(1,1,HID_REPORT_TYPE_FEATURE,feature,31)==0);
    /* Run the pinned TinyUSB HID driver, including its Report ID insertion. */
    hidd_init();
    CHECK(hidd_open(0,(const tusb_desc_interface_t *)(xinput_configuration+49),25)==25);
    tusb_control_request_t hr={.bmRequestType=0xa1,.bRequest=1,.wValue=0x0301,.wIndex=1,.wLength=32};
    CHECK(hidd_control_xfer_cb(0,CONTROL_STAGE_SETUP,&hr));
    CHECK(control_len==32 && ctrl[0]==1 && !memcmp(ctrl+1,"P2W",3) && ctrl[12]==2);
    /* Execute SET_REPORT ID stripping and GET_REPORT ID insertion in TinyUSB. */
    uint8_t command[32]={2,'P','2','C',1,CFG_BEGIN,0x34,0x12};
    uint16_t command_crc=config_crc16(command+1,29);command[30]=(uint8_t)command_crc;command[31]=(uint8_t)(command_crc>>8);
    hr=(tusb_control_request_t){.bmRequestType=0x21,.bRequest=9,.wValue=0x0302,.wIndex=1,.wLength=32};
    CHECK(hidd_control_xfer_cb(0,CONTROL_STAGE_SETUP,&hr));CHECK(control_len==32);
    memcpy(control_out,command,32);CHECK(hidd_control_xfer_cb(0,CONTROL_STAGE_DATA,&hr));
    CHECK(!usb_xinput_config()->staged);CHECK(hidd_control_xfer_cb(0,CONTROL_STAGE_ACK,&hr));
    CHECK(usb_xinput_config()->staged && usb_xinput_config()->token==0x1234);
    hr=(tusb_control_request_t){.bmRequestType=0xa1,.bRequest=1,.wValue=0x0303,.wIndex=1,.wLength=32};
    CHECK(hidd_control_xfer_cb(0,CONTROL_STAGE_SETUP,&hr));
    CHECK(ctrl[0]==3 && ctrl[5]==CFG_BEGIN && ctrl[6]==0x34 && ctrl[7]==0x12 && ctrl[10]==CFG_OK);
    CHECK(config_crc16(ctrl+1,29)==(uint16_t)(ctrl[30]|ctrl[31]<<8));
    /* A short SET or bad checksum must not mutate the active configuration. */
    receiver_config_t original=b.config;
    hr=(tusb_control_request_t){.bmRequestType=0x21,.bRequest=9,.wValue=0x0302,.wIndex=1,.wLength=8};
    CHECK(hidd_control_xfer_cb(0,CONTROL_STAGE_SETUP,&hr));memset(control_out,0,8);
    CHECK(hidd_control_xfer_cb(0,CONTROL_STAGE_ACK,&hr));CHECK(!memcmp(&original,&b.config,sizeof(original)));
    hr.wLength=32;CHECK(hidd_control_xfer_cb(0,CONTROL_STAGE_SETUP,&hr));command[5]=CFG_DEFAULTS;
    memcpy(control_out,command,32);CHECK(hidd_control_xfer_cb(0,CONTROL_STAGE_ACK,&hr));
    CHECK(usb_xinput_config()->response[9]==CFG_BAD_PACKET && !memcmp(&original,&b.config,sizeof(original)));
    hr=(tusb_control_request_t){.bmRequestType=0x81,.bRequest=6,.wValue=0x2200,.wIndex=1,.wLength=STATUS_HID_DESCRIPTOR_SIZE};
    CHECK(hidd_control_xfer_cb(0,CONTROL_STAGE_SETUP,&hr));
    CHECK(control_len==STATUS_HID_DESCRIPTOR_SIZE && !memcmp(ctrl,hid,STATUS_HID_DESCRIPTOR_SIZE));
    const tusb_desc_device_t *dev=(const tusb_desc_device_t *)tud_descriptor_device_cb();
    CHECK(dev->bMaxPacketSize0==64 && dev->bNumConfigurations==1);
    CHECK(tud_descriptor_configuration_cb(1)==NULL);
    const uint16_t *str=tud_descriptor_string_cb(0xee,0);
    CHECK(str[0]==0x0312 && str[1]=='M' && str[8]==XINPUT_VENDOR_CODE);
    str=tud_descriptor_string_cb(3,0); CHECK(str[0]==0x0322 && str[1]=='0');
    CHECK(tud_descriptor_string_cb(4,0)==NULL);
    tusb_control_request_t r={.bmRequestType=0xc0,.bRequest=XINPUT_VENDOR_CODE,.wIndex=4,.wLength=40};
    CHECK(tud_vendor_control_xfer_cb(0,CONTROL_STAGE_SETUP,&r));
    CHECK(control_len==40 && !memcmp(ctrl+18,"XUSB10",6) && ctrl[16]==0 && ctrl[17]==1);
    r.bmRequestType=0x40; CHECK(!tud_vendor_control_xfer_cb(0,CONTROL_STAGE_SETUP,&r));
    r.bmRequestType=0xc0; r.wIndex=5; CHECK(!tud_vendor_control_xfer_cb(0,CONTROL_STAGE_SETUP,&r));
    r.bRequest=1; r.wIndex=0; r.wLength=4; CHECK(tud_vendor_control_xfer_cb(0,CONTROL_STAGE_SETUP,&r));
    CHECK(control_len==4 && ctrl[0]==0x88 && ctrl[3]==8);
    r.bmRequestType=0x41; r.bRequest=0; r.wLength=0;
    CHECK(tud_vendor_control_xfer_cb(0,CONTROL_STAGE_SETUP,&r));
    r.wLength=1; CHECK(!tud_vendor_control_xfer_cb(0,CONTROL_STAGE_SETUP,&r));
    mounted=true; tud_mount_cb(); bridge_connect(&b,PAD_XSX);
    pad_input_t input={.buttons=1,.brake=512}; bridge_input(&b,&input);
    usb_xinput_service(1); CHECK(input_count==1 && sent[1]==20 && !sent[3]);
    input.buttons=2; bridge_input(&b,&input); usb_xinput_service(5);
    CHECK(input_count==1 && !inflight[3]); complete(d);
    usb_xinput_service(6); CHECK(input_count==2 && sent[3]==0x20 && sent[4]==128);
    /* Disconnect while IN is occupied: release report gets the next free slot. */
    bridge_disconnect(&b); usb_xinput_service(7); CHECK(input_count==2);
    CHECK(inflight[3]==0x20); complete(d); usb_xinput_service(8);
    CHECK(input_count==3 && sent[3]==0 && b.force_neutral==false); complete(d);
    bridge_connect(&b,PAD_STADIA); bridge_input(&b,&input);
    fail_xfer=true; usb_xinput_service(9); CHECK(b.force_neutral && !busy && input_count==3);
    fail_xfer=false; usb_xinput_service(10); CHECK(!b.force_neutral && !sent[3]); complete(d);
    unsigned before=input_count;
    usb_xinput_service(11); CHECK(input_count==before);
    usb_xinput_service(12); CHECK(input_count==before+1); complete(d);
    uint8_t strong,weak; bridge_take_rumble(&b,false,&strong,&weak);
    const uint8_t on[8]={0,8,0,200,80}; memcpy(rx,on,8);
    d->xfer_cb(0,0x03,XFER_RESULT_SUCCESS,8); CHECK(b.strong==200 && b.weak==80 && output_count==2);
    rx[1]=7; d->xfer_cb(0,0x03,XFER_RESULT_SUCCESS,8); CHECK(b.strong==200 && output_count==3);
    memcpy(rx,on,8); d->xfer_cb(0,0x03,XFER_RESULT_FAILED,8); CHECK(output_count==4);
    r=(tusb_control_request_t){.bmRequestType=0x21,.bRequest=9,.wValue=0x0200,.wIndex=0,.wLength=8};
    CHECK(d->control_xfer_cb(0,CONTROL_STAGE_SETUP,&r));
    memcpy(control_out,on,7);
    CHECK(d->control_xfer_cb(0,CONTROL_STAGE_ACK,&r)); CHECK(b.strong==200);
    CHECK(d->control_xfer_cb(0,CONTROL_STAGE_SETUP,&r));
    memset(control_out,0,8); ((uint8_t *)control_out)[1]=8;
    CHECK(d->control_xfer_cb(0,CONTROL_STAGE_DATA,&r)); CHECK(b.strong==200);
    CHECK(d->control_xfer_cb(0,CONTROL_STAGE_ACK,&r)); CHECK(b.strong==0 && b.stop_pending);
    r.wLength=9; CHECK(!d->control_xfer_cb(0,CONTROL_STAGE_SETUP,&r));
    r=(tusb_control_request_t){.bmRequestType=0xa1,.bRequest=1,.wValue=0x0100,.wLength=20};
    CHECK(d->control_xfer_cb(0,CONTROL_STAGE_SETUP,&r)); CHECK(control_len==20 && ctrl[1]==20);
    r.wIndex=1; CHECK(!d->control_xfer_cb(0,CONTROL_STAGE_SETUP,&r));
    r=(tusb_control_request_t){.bmRequestType=0x81,.bRequest=6,.wValue=0x2100,.wLength=17};
    CHECK(d->control_xfer_cb(0,CONTROL_STAGE_SETUP,&r)); CHECK(control_len==17 && ctrl[0]==17);
    memcpy(rx,on,8); d->xfer_cb(0,0x03,XFER_RESULT_SUCCESS,8);
    suspended=true; tud_suspend_cb(false); CHECK(!b.usb_active && !b.strong && !b.weak);
    suspended=false; tud_resume_cb(); CHECK(b.usb_active && b.force_neutral);
    d->reset(0); CHECK(!b.usb_active); unsigned previous=input_count;
    usb_xinput_service(30); CHECK(input_count==previous);
    tud_umount_cb(); CHECK(!b.usb_active);
    printf("USB: %u checks passed\n",checks); return 0;
}
