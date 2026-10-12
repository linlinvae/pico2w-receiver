#include "peer_store.h"
#include "board_config.h"
#include <string.h>

static uint32_t get32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static void put32(uint8_t *p, uint32_t v)
{ for (unsigned i=0; i<4; ++i) p[i]=(uint8_t)(v>>(8*i)); }
static uint32_t crc32(const uint8_t *p, unsigned n)
{
    uint32_t crc = UINT32_MAX;
    while (n--) {
        crc ^= *p++;
        for (unsigned b=0; b<8; ++b) crc = (crc>>1) ^ ((crc&1) ? 0xEDB88320UL : 0);
    }
    return ~crc;
}
static bool usable_address(uint8_t type, const uint8_t *address)
{
    if (!address || type>1 || (type==1 && (address[5]&0xC0)!=0xC0)) return false;
    unsigned zeros=0, ones=0;
    for (unsigned i=0; i<6; ++i) { zeros += address[i]==0; ones += address[i]==0xFF; }
    return zeros!=6 && ones!=6;
}
static bool decode(peer_store_t *s, const uint8_t *p, uint8_t bank)
{
    if (memcmp(p,"STD1",4) || p[4]!=1 || p[5]>1 || p[7]!=0 ||
        get32(p+24)!=crc32(p,24) || get32(p+28)!=0x54494D43UL) return false;
    if (p[5] && !usable_address(p[6],p+12)) return false;
    memset(s,0,sizeof(*s));
    s->loaded=true; s->valid=p[5]!=0; s->address_type=p[6]; s->bank=bank;
    s->generation=get32(p+8); memcpy(s->address,p+12,6);
    return true;
}
bool peer_store_load(peer_store_t *s)
{
    uint32_t words[PEER_STORE_RECORD_BYTES/4];
    peer_store_t candidate;
    bool found=false;
    memset(s,0,sizeof(*s)); s->bank=1;
    for (uint8_t bank=0; bank<2; ++bank) {
        if (peer_flash_read(BOARD_PEER_STORE_ADDR+bank*PEER_STORE_PAGE_BYTES,
                            words,sizeof(words))!=0) {
            memset(s,0,sizeof(*s)); return false;
        }
        if (!decode(&candidate,(const uint8_t *)words,bank)) continue;
        uint32_t distance=candidate.generation-s->generation;
        if (!found || (distance!=0 && distance<0x80000000UL)) { *s=candidate; found=true; }
    }
    s->loaded=true;
    return true;
}
static bool commit(peer_store_t *s, bool valid, uint8_t type, const uint8_t *address)
{
    if (!s->loaded) return false;
    uint32_t words[PEER_STORE_RECORD_BYTES/4], verify[PEER_STORE_RECORD_BYTES/4];
    uint8_t *p=(uint8_t *)words;
    memset(p,0,sizeof(words)); memcpy(p,"STD1",4);
    p[4]=1; p[5]=valid; p[6]=type; put32(p+8,s->generation+1);
    if (valid) memcpy(p+12,address,6);
    put32(p+24,crc32(p,24)); put32(p+28,0x54494D43UL);
    uint8_t bank=s->bank^1;
    uint32_t slot=BOARD_PEER_STORE_ADDR+bank*PEER_STORE_PAGE_BYTES;
    /* Never erase the newest record. The final marker commits a complete CRC
     * record; a power cut leaves either the old or the new generation usable. */
    if (peer_flash_erase(slot,PEER_STORE_PAGE_BYTES)!=0 ||
        peer_flash_write(slot,words,28)!=0 ||
        peer_flash_write(slot+28,p+28,4)!=0 ||
        peer_flash_read(slot,verify,sizeof(verify))!=0 ||
        memcmp(words,verify,sizeof(words))) return false;
    return decode(s,(const uint8_t *)verify,bank);
}
bool peer_store_save(peer_store_t *s, uint8_t type, const uint8_t address[6])
{
    if (!usable_address(type,address)) return false;
    if (s->valid && s->address_type==type && !memcmp(s->address,address,6)) return true;
    return commit(s,true,type,address);
}
bool peer_store_clear(peer_store_t *s)
{
    if (!s->loaded) return false;
    if (!s->valid) return true;
    return commit(s,false,0,NULL);
}
