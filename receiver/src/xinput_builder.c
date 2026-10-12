/* Wire format: Microsoft MS-XUSBI 3.2.5.1.1 and 3.2.5.2.1.
 * Application mapping code is original to this local Apache-2.0 fork. */
#include "xinput_builder.h"
#include <string.h>
#include <stddef.h>

static void put16(uint8_t *p, int32_t n)
{ uint16_t value=(uint16_t)n; p[0]=(uint8_t)value; p[1]=(uint8_t)(value>>8); }
static int32_t axis(uint8_t value, bool invert)
{
    int32_t delta=(int32_t)value-128;
    if (!invert) return delta<0 ? delta*256 : delta*32767/127;
    /* XInput is positive up; preserve exact zero and both signed endpoints. */
    return delta<0 ? -delta*32767/128 : -delta*32768/127;
}
void xinput_build_report(uint32_t buttons, const uint8_t analog[6], uint8_t out[20])
{
    /* JP: A B X Y LB RB LT RT Back Start LS RS Up Down Left Right Guide. */
    static const uint16_t bits[17]={
        0x1000,0x2000,0x4000,0x8000,0x0100,0x0200,0,0,
        0x0020,0x0010,0x0040,0x0080,0x0001,0x0002,0x0004,0x0008,0x0400
    };
    uint16_t mask=0;
    for (unsigned i=0;i<17;++i) if (buttons&((uint32_t)1<<i)) mask|=bits[i];
    memset(out,0,20); out[1]=20; put16(out+2,mask);
    out[4]=analog[4]; out[5]=analog[5];
    put16(out+6,axis(analog[0],false)); put16(out+8,axis(analog[1],true));
    put16(out+10,axis(analog[2],false)); put16(out+12,axis(analog[3],true));
}
void xinput_build_neutral_report(uint8_t out[20])
{
    const uint8_t analog[6]={128,128,128,128,0,0};
    xinput_build_report(0,analog,out);
}
bool xinput_parse_rumble(const uint8_t *data, uint16_t len, uint8_t *left, uint8_t *right)
{
    if (!data || !left || !right || len!=8 || data[0]!=0 || data[1]!=8 || data[2]!=0)
        return false;
    *left=data[3]; *right=data[4]; return true;
}
