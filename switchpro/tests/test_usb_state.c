/* Run actual USB buffering code against register variables, not an MCU. */
#ifdef CFG_CH582
#include "CH58x_common.h"
#else
#include "CH59x_common.h"
#endif
#include <stdio.h>
#include <stdbool.h>

#undef R8_UEP0_CTRL
#undef R8_UEP0_T_LEN
#undef R8_UEP1_CTRL
#undef R8_UEP2_CTRL
#undef R8_UEP2_T_LEN
#undef R8_UEP3_CTRL
#undef R8_UEP3_T_LEN
#undef R8_USB_DEV_AD
#undef R8_USB_INT_FG
#undef R8_USB_INT_ST
#undef R8_USB_MIS_ST
#undef R8_USB_RX_LEN
static uint8_t reg0, len0, reg1, reg2, len2, reg3, len3, addr, flags, status, mis, rxlen;
#define R8_UEP0_CTRL reg0
#define R8_UEP0_T_LEN len0
#define R8_UEP1_CTRL reg1
#define R8_UEP2_CTRL reg2
#define R8_UEP2_T_LEN len2
#define R8_UEP3_CTRL reg3
#define R8_UEP3_T_LEN len3
#define R8_USB_DEV_AD addr
#define R8_USB_INT_FG flags
#define R8_USB_INT_ST status
#define R8_USB_MIS_ST mis
#define R8_USB_RX_LEN rxlen
static bool irq_disabled;
#define PFIC_DisableIRQ(irq) (irq_disabled = true)
#define PFIC_EnableIRQ(irq) (irq_disabled = false)

#include "../ch582/src/sinput_usb.c"
#if USB_GAMEPAD_XINPUT
#include "../ch582/src/xinput_builder.c"
#endif
volatile uint32_t g_sys_tick_ms;
uint8_t *pEP0_RAM_Addr, *pEP1_RAM_Addr, *pEP2_RAM_Addr, *pEP3_RAM_Addr;
void USB_DeviceInit(void) {}
void platform_reboot(void) {}
void platform_get_unique_id(uint8_t uid[6]) { const uint8_t id[6]={1,2,3,0xA4,5,6}; memcpy(uid,id,6); }

static int s_fail, s_calls;
static bool s_snapshot_ok;
static void check(const char *name, bool ok)
{
    printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) s_fail = 1;
}
static void on_output(uint8_t *data, uint16_t len)
{
    ++s_calls;
    /* Inject the next interrupt during this callback. The original packet
     * must remain intact, and the next packet must remain pending. */
    memset(pEP3_OUT_DataBuf, 0xCC, 7);
    HID_OUT_Deal(7);
    s_snapshot_ok = !irq_disabled && len == 7 && data[0] == 3 && data[3] == 0;
}

static void setup(uint8_t type, uint8_t request, uint16_t value, uint16_t index, uint16_t length)
{
    memset(EP0_Databuf,0,EP0_SIZE);
    pSetupReqPak->bRequestType=type;
    pSetupReqPak->bRequest=request;
    pSetupReqPak->wValue=value;
    pSetupReqPak->wIndex=index;
    pSetupReqPak->wLength=length;
    flags=RB_UIF_TRANSFER;
    status=RB_UIS_SETUP_ACT | MASK_UIS_TOKEN;
    USB_DevTransProcess();
}
static void token(uint8_t token_type, uint8_t bytes)
{
    flags=RB_UIF_TRANSFER; status=token_type|RB_UIS_TOG_OK; rxlen=bytes;
    USB_DevTransProcess();
}
static bool stalled(void) { return (reg0&MASK_UEP_T_RES)==UEP_T_RES_STALL; }
static void test_control(void)
{
    setup(0x80,USB_GET_DESCRIPTOR,0x0200,0,255);
    uint8_t assembled[256];
    unsigned used=len0;
    memcpy(assembled,EP0_Databuf,len0);
    while (g_req_len) {
        token(UIS_TOKEN_IN,0);
        memcpy(assembled+used,EP0_Databuf,len0); used+=len0;
    }
    check("configuration control transfer spans packets correctly", used==sizeof(cfg_descr) && !memcmp(assembled,cfg_descr,used));
    token(UIS_TOKEN_OUT,0);
    check("control IN ends at host status stage", !g_control_in && (reg0&MASK_UEP_T_RES)==UEP_T_RES_NAK);
    setup(0x80,USB_GET_DESCRIPTOR,0x0200,0,9);
    check("short configuration request returns only header", len0==9 && g_req_len==0 && !memcmp(EP0_Databuf,cfg_descr,9));
    setup(0x80,USB_GET_DESCRIPTOR,0x03FE,0,255);
    check("unknown string stalls without using old descriptor", stalled() && !g_control_in);
    setup(0xA1,0x21,0,CDC_CONTROL_INTERFACE,64);
    check("CDC line coding response is exactly seven bytes", !stalled() && len0==7 && !memcmp(EP0_Databuf,cdc_line_coding,7));
    setup(0xA1,0x21,0,CDC_CONTROL_INTERFACE,3);
    check("CDC response honors short host request", len0==3);
    setup(0x21,0x20,0,CDC_CONTROL_INTERFACE,7);
    check("CDC SET line coding waits for data before status ACK", !stalled() && g_control_out==CONTROL_OUT_CDC && (reg0&MASK_UEP_T_RES)==UEP_T_RES_NAK);
    const uint8_t coding[7]={0x80,0x25,0,0,0,0,8};
    memcpy(EP0_Databuf,coding,7); token(UIS_TOKEN_OUT,7);
    check("CDC data OUT updates line coding and ACKs status", !memcmp(cdc_line_coding,coding,7) && g_control_out==CONTROL_OUT_NONE && (reg0&MASK_UEP_T_RES)==UEP_T_RES_ACK);
    setup(0x21,0x20,0,CDC_CONTROL_INTERFACE,8);
    check("CDC rejects invalid data length", stalled());
    setup(0x21,0x21,0,CDC_CONTROL_INTERFACE,7);
    check("CDC rejects reversed GET direction", stalled());
    setup(0xA1,0x21,0,CDC_CONTROL_INTERFACE|0x100,7);
    check("class request validates full interface index", stalled());
    g_usb_config=1;
    setup(0x80,USB_GET_CONFIGURATION,0,0,64);
    check("GET configuration does not leak setup buffer", len0==1 && EP0_Databuf[0]==1);
    setup(0x80,USB_GET_STATUS,0,0,1);
    check("GET status honors one-byte length", len0==1);
    addr=0;
    setup(0,USB_SET_ADDRESS,17,0,0);
    check("USB address unchanged before status ACK", addr==0);
    token(UIS_TOKEN_IN,0);
    check("USB address applies after status ACK", addr==17);
#if USB_GAMEPAD_XINPUT
    unsigned offset=9, iface_count=0;
    bool layout_ok=true;
    while (offset<sizeof(cfg_descr)) {
        uint8_t bytes=cfg_descr[offset];
        if (bytes<2 || offset+bytes>sizeof(cfg_descr)) { layout_ok=false; break; }
        if (cfg_descr[offset+1]==4) ++iface_count;
        offset+=bytes;
    }
    check("XUSB composite descriptor walks through all three interfaces", layout_ok && offset==115 && iface_count==3);
    check("XUSB interface uses vendor class and local EP3 addresses", cfg_descr[14]==0xFF && cfg_descr[15]==0x5D && cfg_descr[16]==1 && cfg_descr[24]==0x83 && cfg_descr[31]==3);
    setup(0x80,USB_GET_DESCRIPTOR,0x03EE,0,255);
    check("Microsoft OS string announces request code", len0==18 && !memcmp(EP0_Databuf,xinput_ms_string,18));
    setup(0xC0,XINPUT_MS_VENDOR_CODE,0,4,255);
    check("Microsoft compatible ID binds only XUSB interface", len0==40 && !memcmp(EP0_Databuf,xinput_ms_compatible,40) && EP0_Databuf[16]==0 && EP0_Databuf[17]==1);
    setup(0xC0,XINPUT_MS_VENDOR_CODE,0,4,16);
    check("Microsoft compatible header honors short requests", len0==16 && g_req_len==0);
    setup(0xC0,XINPUT_MS_VENDOR_CODE,0,5,255);
    check("unsupported Microsoft feature stalls", stalled());
    setup(0xC0,1,0,0,4);
    check("XUSB device ID has reserved nibble clear", len0==4 && EP0_Databuf[0]==1 && EP0_Databuf[3]==4);
    setup(0x41,0,0,0,0);
    check("wired XUSB SET CONTROL accepts interface zero", !stalled() && len0==0);
    setup(0x81,USB_GET_DESCRIPTOR,0x2200,0,255);
    check("XUSB does not expose a generic HID report descriptor", stalled());
    setup(0xA1,1,0x0100,0,20);
    check("XUSB GET report returns current neutral state", len0==20 && EP0_Databuf[0]==0 && EP0_Databuf[1]==20 && EP0_Databuf[2]==0);
    uint8_t next[20]={0,20,0x10};
    g_usb_ready=true; reg3=UEP_R_RES_ACK|UEP_T_RES_NAK;
    SInputUSB_SendReport(next,20);
    setup(0xA1,1,0x0100,0,255);
    check("XUSB GET report caches successful fresh input", len0==20 && EP0_Databuf[2]==0x10);
    setup(0x21,9,0x0200,0,8);
    check("XUSB SET rumble waits for eight-byte data", !stalled() && g_control_out==CONTROL_OUT_XINPUT && (reg0&MASK_UEP_T_RES)==UEP_T_RES_NAK);
    const uint8_t rumble[8]={0,8,0,99,42,0,0,0};
    memcpy(EP0_Databuf,rumble,8); token(UIS_TOKEN_OUT,8);
    check("XUSB control rumble queues original bytes", g_hid_out_pending && g_hid_out_len==8 && !memcmp(g_hid_out_data,rumble,8) && (reg3&MASK_UEP_R_RES)==UEP_R_RES_NAK);
    setup(0x21,9,0x0200,0,8);
    check("pending XUSB rumble is protected against control overwrite", stalled() && !memcmp(g_hid_out_data,rumble,8));
    g_output_cb=NULL; SInputUSB_ProcessOutput();
    setup(0x21,9,0x0200,0,8);
    memcpy(EP0_Databuf,rumble,7); token(UIS_TOKEN_OUT,7);
    check("truncated XUSB control rumble stalls without queueing", stalled() && !g_hid_out_pending);
    reg3=UEP_R_RES_ACK|UEP_T_RES_NAK;
    xinput_last_send_ms=g_sys_tick_ms=100;
    SInputUSB_Poll(103);
    check("XUSB idle report waits for four milliseconds", (reg3&MASK_UEP_T_RES)==UEP_T_RES_NAK);
    g_sys_tick_ms=104; SInputUSB_Poll(104);
    check("XUSB idle report sends cached state at four milliseconds", len3==20 && pEP3_IN_DataBuf[2]==0x10 && xinput_last_send_ms==104);
    g_sys_tick_ms=108; SInputUSB_Poll(108);
    check("XUSB idle report respects busy endpoint", xinput_last_send_ms==104);
    g_usb_ready=false; reg3=UEP_R_RES_ACK|UEP_T_RES_NAK;
    SInputUSB_Poll(108);
    check("XUSB idle report respects suspended state", xinput_last_send_ms==104 && (reg3&MASK_UEP_T_RES)==UEP_T_RES_NAK);
    flags=RB_UIF_BUS_RST; USB_DevTransProcess();
    check("bus reset clears XUSB cached pressed buttons", xinput_last_report[2]==0 && xinput_last_report[1]==20);
#endif
}

int main(void)
{
    SInputUSB_Init();
    pEP0_RAM_Addr = EP0_Databuf;
    pEP1_RAM_Addr = EP1_Databuf;
    pEP2_RAM_Addr = EP2_Databuf;
    pEP3_RAM_Addr = EP3_Databuf;
    uint8_t report[64] = { USB_GAMEPAD_XINPUT ? 0 : 1, USB_GAMEPAD_XINPUT ? 20 : 0, 0 };
    const uint16_t report_bytes=USB_GAMEPAD_XINPUT ? 20 : 64;
    R8_UEP3_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    check("unconfigured USB does not accept input", !SInputUSB_SendReport(report,report_bytes) && !irq_disabled);
    g_usb_config = 1;
    g_usb_ready = true;
    check("configured input arms IN endpoint", SInputUSB_SendReport(report,report_bytes) && len3 == report_bytes && !irq_disabled);
    report[2] = 2;
    check("busy IN cannot overwrite feature or input", !SInputUSB_SendReport(report,report_bytes) && pEP3_IN_DataBuf[2] == 0);
    R8_UEP3_CTRL = UEP_R_RES_ACK | UEP_T_RES_STALL;
    check("halted IN is not implicitly resumed", !SInputUSB_SendReport(report,report_bytes));
    R8_UEP3_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    uint8_t stop[7] = { 3, 1, 2, 0, 0, 0, 0 };
    memcpy(pEP3_OUT_DataBuf,stop,7);
    HID_OUT_Deal(7);
    check("pending OUT backpressures host", g_hid_out_pending && (reg3 & MASK_UEP_R_RES) == UEP_R_RES_NAK);
    memset(pEP3_OUT_DataBuf,0xAA,7);
    HID_OUT_Deal(7);
    check("pending stop cannot be overwritten", g_hid_out_data[0] == 3 && g_hid_out_data[3] == 0);
    SInputUSB_SetOutputCallback(on_output);
    SInputUSB_ProcessOutput();
    check("callback uses stable snapshot despite next interrupt", s_calls == 1 && s_snapshot_ok);
    check("next OUT survives callback", g_hid_out_pending && g_hid_out_data[0] == 0xCC);
    g_output_cb = NULL;
    SInputUSB_ProcessOutput();
    check("processed OUT rearms endpoint", !g_hid_out_pending && (reg3 & MASK_UEP_R_RES) == UEP_R_RES_ACK);
    g_hid_out_pending = true;
    R8_USB_INT_FG = RB_UIF_BUS_RST;
    USB_DevTransProcess();
    check("bus reset clears configuration and stale output", !g_usb_ready && g_usb_config == 0 && !g_hid_out_pending);
    g_usb_config = 1;
    g_usb_ready = true;
    R8_USB_INT_FG = RB_UIF_SUSPEND;
    R8_USB_MIS_ST = RB_UMS_SUSPEND;
    USB_DevTransProcess();
    check("suspend stops input submission", !SInputUSB_SendReport(report,report_bytes));
    R8_USB_INT_FG = RB_UIF_SUSPEND;
    R8_USB_MIS_ST = 0;
    USB_DevTransProcess();
    check("configured resume restores readiness", SInputUSB_IsReady());
    test_control();
    printf("RESULT: %s\n", s_fail ? "FAIL" : "PASS");
    return s_fail;
}
