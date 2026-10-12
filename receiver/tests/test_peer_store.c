#include "peer_store.h"
#include "board_config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static uint8_t flash[0x8000];
static int failed, mutations, fail_mutation, reads, fail_read;
static uint8_t first[] = {0x23,0x2e,0x29,0x04,0xe0,0xd8};
static uint8_t second[] = {1,2,3,4,5,6};
static void check(const char *name, bool ok)
{ printf("%s %s\n", ok ? "PASS" : "FAIL", name); failed |= !ok; }
static void bounds(uint32_t address, uint32_t bytes)
{
    if (address < BOARD_PEER_STORE_ADDR || address+bytes > BOARD_PEER_STORE_ADDR+512) abort();
}
uint32_t peer_flash_read(uint32_t address, void *data, uint32_t bytes)
{
    bounds(address,bytes);
    if (++reads==fail_read) return 1;
    memcpy(data,flash+address,bytes); return 0;
}
uint32_t peer_flash_erase(uint32_t address, uint32_t bytes)
{
    bounds(address,bytes);
    if (address%256 || bytes!=256) abort();
    if (++mutations==fail_mutation) return 1;
    memset(flash+address,0xff,bytes); return 0;
}
uint32_t peer_flash_write(uint32_t address, const void *data, uint32_t bytes)
{
    bounds(address,bytes);
    if (++mutations==fail_mutation) return 1;
    const uint8_t *p=data;
    for (uint32_t i=0;i<bytes;++i) {
        if ((flash[address+i]&p[i])!=p[i]) abort(); /* Real NOR cannot set erased bits. */
        flash[address+i]&=p[i];
    }
    return 0;
}
static bool is_peer(const peer_store_t *s,const uint8_t *address)
{ return s->valid && !memcmp(s->address,address,6); }
static uint32_t crc(const uint8_t *p,unsigned n)
{
    uint32_t c=UINT32_MAX;
    while (n--) { c^=*p++; for (unsigned b=0;b<8;++b) c=(c>>1)^((c&1)?0xEDB88320UL:0); }
    return ~c;
}
static void put32(uint8_t *p,uint32_t n)
{ for (unsigned i=0;i<4;++i) p[i]=(uint8_t)(n>>(8*i)); }
int main(void)
{
    peer_store_t s,reboot;
    memset(flash,0xff,sizeof(flash));
    check("erased journal loads as unpaired without writing",peer_store_load(&s)&&!s.valid&&s.loaded&&mutations==0);
    check("first public identity commits successfully",peer_store_save(&s,0,first)&&is_peer(&s,first)&&s.generation==1);
    check("committed peer survives a fresh boot",peer_store_load(&reboot)&&is_peer(&reboot,first));
    int before=mutations, before_read=reads;
    check("same peer causes no Flash wear or read",peer_store_save(&s,0,first)&&mutations==before&&reads==before_read);
    check("second peer alternates bank",peer_store_save(&s,0,second)&&s.bank!=reboot.bank&&s.generation==2);
    check("newest peer wins on boot",peer_store_load(&reboot)&&is_peer(&reboot,second));
    uint8_t committed[sizeof(flash)]; memcpy(committed,flash,sizeof(flash));
    for (int step=1;step<=3;++step) {
        memcpy(flash,committed,sizeof(flash)); peer_store_load(&s);
        fail_mutation=mutations+step;
        bool result=peer_store_save(&s,0,first);
        fail_mutation=0;
        check(step==1?"erase failure retains old peer":step==2?"power cut before body retains old peer":"power cut before commit retains old peer",
              !result&&is_peer(&s,second)&&peer_store_load(&reboot)&&is_peer(&reboot,second));
    }
    memcpy(flash,committed,sizeof(flash)); peer_store_load(&s);
    fail_read=reads+1;
    check("readback error cannot claim successful save",!peer_store_save(&s,0,first)&&is_peer(&s,second));
    fail_read=0;
    check("a fully committed write still recovers after readback failure",peer_store_load(&reboot)&&is_peer(&reboot,first));
    memcpy(flash,committed,sizeof(flash)); peer_store_load(&s);
    flash[BOARD_PEER_STORE_ADDR+s.bank*256+12]^=1;
    check("CRC corruption falls back to prior committed page",peer_store_load(&reboot)&&is_peer(&reboot,first));
    memcpy(flash,committed,sizeof(flash)); peer_store_load(&s);
    flash[BOARD_PEER_STORE_ADDR+s.bank*256+28]=0xff;
    check("missing commit marker falls back to old peer",peer_store_load(&reboot)&&is_peer(&reboot,first));
    memcpy(flash,committed,sizeof(flash)); peer_store_load(&s);
    flash[BOARD_PEER_STORE_ADDR+s.bank*256+4]=2;
    check("unknown record version is rejected",peer_store_load(&reboot)&&is_peer(&reboot,first));
    memcpy(flash,committed,sizeof(flash)); peer_store_load(&s);
    for (int step=1;step<=3;++step) {
        memcpy(flash,committed,sizeof(flash)); peer_store_load(&s); fail_mutation=mutations+step;
        bool result=peer_store_clear(&s); fail_mutation=0;
        check(step==1?"failed erase cannot forget peer":step==2?"partial tombstone cannot forget peer":"uncommitted tombstone cannot forget peer",
              !result&&peer_store_load(&reboot)&&is_peer(&reboot,second));
    }
    memcpy(flash,committed,sizeof(flash)); peer_store_load(&s);
    check("forget commits a tombstone",peer_store_clear(&s)&&!s.valid);
    check("forget survives reboot without resurrecting older page",peer_store_load(&reboot)&&!reboot.valid&&reboot.generation==3);
    before=mutations;
    check("repeated forget does not rewrite Flash",peer_store_clear(&s)&&mutations==before);
    check("new peer can be saved after forget",peer_store_save(&s,0,first)&&peer_store_load(&reboot)&&is_peer(&reboot,first));
    before=mutations;
    uint8_t zero[6]={0},ones[6]={255,255,255,255,255,255};
    check("invalid zero and broadcast identities cannot be saved",!peer_store_save(&s,0,zero)&&!peer_store_save(&s,0,ones)&&mutations==before);
    check("private rotating addresses cannot be persisted as stable identity",!peer_store_save(&s,2,first)&&!peer_store_save(&s,1,second));
    check("random static identity is accepted",peer_store_save(&s,1,first)&&s.address_type==1);
    /* Manufacture a valid prior generation near wrap, then commit through zero. */
    memset(flash,0xff,sizeof(flash)); s.loaded=true; s.valid=false; s.bank=1; s.generation=0;
    peer_store_save(&s,0,first);
    uint8_t *record=flash+BOARD_PEER_STORE_ADDR;
    put32(record+8,UINT32_MAX-1); put32(record+24,crc(record,24));
    peer_store_load(&s); peer_store_save(&s,0,second);
    check("generation UINT32_MAX is newer than its predecessor",peer_store_load(&reboot)&&is_peer(&reboot,second)&&reboot.generation==UINT32_MAX);
    peer_store_save(&s,0,first);
    check("generation zero after wrap remains newest",peer_store_load(&reboot)&&is_peer(&reboot,first)&&reboot.generation==0);
    fail_read=reads+1;
    check("storage read failure blocks destructive writes",!peer_store_load(&s)&&!s.loaded&&!peer_store_save(&s,0,second)&&!peer_store_clear(&s));
    fail_read=0;
    check("read can recover on a later attempt",peer_store_load(&s)&&is_peer(&s,first));
    return failed;
}
