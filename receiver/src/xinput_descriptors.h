#ifndef XINPUT_DESCRIPTORS_H
#define XINPUT_DESCRIPTORS_H
/* Protocol layouts implemented from Microsoft MS-XUSBI sections 2.2.4.6 and
 * 3.2.1.1. Endpoint addresses are local WCH allocations, not a retail device.
 * Interface 0: wired XUSB; interfaces 1/2: CDC. One gamepad, no audio/security. */
#define XINPUT_MS_VENDOR_CODE 0x20
#define GAMEPAD_INTERFACE 0
#define CDC_CONTROL_INTERFACE 1
#define CFG_GAMEPAD_DESCR_OFFSET 18
static const uint8_t cfg_descr[]={
    9,2,115,0,3,1,0,0x80,0x32,
    9,4,0,0,2,0xFF,0x5D,1,2,
    /* XUSB subtype 1; one input report, one output rumble report. */
    17,0x21,0,1,1,0x21,0x83,20,0,0,0,0,0x11,0x03,8,0,0,
    7,5,0x83,3,32,0,4,
    7,5,0x03,3,32,0,8,
    8,0x0B,1,2,2,2,1,0,
    9,4,1,0,1,2,2,1,0,
    5,0x24,0,0x10,1,
    5,0x24,1,0,2,
    4,0x24,2,2,
    5,0x24,6,1,2,
    7,5,0x81,3,64,0,1,
    9,4,2,0,2,0x0A,0,0,0,
    7,5,0x02,2,64,0,0,
    7,5,0x82,2,64,0,0,
};
static const uint8_t xinput_ms_string[]={
    18,3,'M',0,'S',0,'F',0,'T',0,'1',0,'0',0,'0',0,XINPUT_MS_VENDOR_CODE,0
};
static const uint8_t xinput_ms_compatible[]={
    40,0,0,0,0,1,4,0,1,0,0,0,0,0,0,0,
    0,1,'X','U','S','B','1','0',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};
_Static_assert(sizeof(cfg_descr)==115,"XUSB+CDC configuration length");
_Static_assert(sizeof(xinput_ms_compatible)==40,"Microsoft compatible ID length");
_Static_assert(sizeof(xinput_ms_string)==18,"Microsoft OS string length");
#endif
