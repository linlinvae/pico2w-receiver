#include "xinput_builder.h"
#include <stdio.h>
#include <string.h>
static int failed;
static void check(const char *name, bool ok)
{ printf("%s %s\n",ok?"PASS":"FAIL",name); failed|=!ok; }
static int16_t read16(const uint8_t *p)
{ return (int16_t)((uint16_t)p[0]|(uint16_t)p[1]<<8); }
int main(void)
{
    uint8_t report[20], a[]={128,128,128,128,0,0};
    xinput_build_report(0,a,report);
    uint8_t neutral[20]={0,20};
    check("XUSB neutral is exactly 20 bytes with zero axes and triggers",!memcmp(report,neutral,20));
    const uint16_t expected[]={0x1000,0x2000,0x4000,0x8000,0x100,0x200,0,0,0x20,0x10,0x40,0x80,1,2,4,8,0x400,0,0};
    bool buttons=true;
    for (unsigned i=0;i<19;++i) {
        xinput_build_report((uint32_t)1<<i,a,report);
        if ((uint16_t)read16(report+2)!=expected[i]) buttons=false;
    }
    check("all Stadia buttons map to XInput ABXY shoulder system sticks and D-pad bits",buttons);
    xinput_build_report(UINT32_MAX,a,report);
    check("capture Assistant and digital trigger flags cannot set reserved XUSB buttons",(uint16_t)read16(report+2)==0xF7FF);
    a[0]=0; a[1]=0; a[2]=255; a[3]=255; a[4]=17; a[5]=231;
    xinput_build_report(0,a,report);
    check("left/right X axes reach exact signed endpoints",read16(report+6)==-32768&&read16(report+10)==32767);
    check("XInput Y is positive up and negative down",read16(report+8)==32767&&read16(report+12)==-32768);
    check("independent analog triggers keep their full 8-bit values",report[4]==17&&report[5]==231);
    bool monotonic=true;
    int16_t last_x=-32768,last_y=32767;
    for (unsigned i=0;i<256;++i) {
        a[0]=a[1]=(uint8_t)i; xinput_build_report(0,a,report);
        int16_t x=read16(report+6),y=read16(report+8);
        if (x<last_x||y>last_y) monotonic=false;
        last_x=x;last_y=y;
    }
    check("axis conversion is monotonic through all input values",monotonic);
    xinput_build_neutral_report(report);
    check("disconnect neutral clears every button and axis including padding",!memcmp(report,neutral,20));
    uint8_t out[]={0,8,0,255,64,0,0,0},left=0,right=0;
    check("XInput motor packet maps large/small amplitude without attenuation",xinput_parse_rumble(out,8,&left,&right)&&left==255&&right==64);
    out[3]=out[4]=0;
    check("XInput zero motor packet preserves stop",xinput_parse_rumble(out,8,&left,&right)&&left==0&&right==0);
    out[0]=1; left=10; right=20;
    check("player LED packet cannot be mistaken for rumble",!xinput_parse_rumble(out,8,&left,&right)&&left==10&&right==20);
    out[0]=0;out[1]=7;
    check("wrong output declared size is rejected",!xinput_parse_rumble(out,8,&left,&right));
    out[1]=8;out[2]=1;
    check("nonzero rumble type is rejected",!xinput_parse_rumble(out,8,&left,&right));
    out[2]=0;
    check("truncated oversized and null motor packets are rejected",!xinput_parse_rumble(out,7,&left,&right)&&!xinput_parse_rumble(out,9,&left,&right)&&!xinput_parse_rumble(NULL,8,&left,&right)&&!xinput_parse_rumble(out,8,NULL,&right));
    return failed;
}
