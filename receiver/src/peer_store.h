#ifndef PEER_STORE_H
#define PEER_STORE_H
#include <stdint.h>
#include <stdbool.h>
#define PEER_STORE_PAGE_BYTES 256u
#define PEER_STORE_RECORD_BYTES 32u
typedef struct {
    uint32_t generation;
    uint8_t address[6], address_type, bank;
    bool valid, loaded;
} peer_store_t;
bool peer_store_load(peer_store_t *store);
bool peer_store_save(peer_store_t *store, uint8_t type, const uint8_t address[6]);
bool peer_store_clear(peer_store_t *store);
/* Adapter uses DataFlash-relative addresses, never CodeFlash. Zero is success. */
uint32_t peer_flash_read(uint32_t address, void *data, uint32_t bytes);
uint32_t peer_flash_erase(uint32_t address, uint32_t bytes);
uint32_t peer_flash_write(uint32_t address, const void *data, uint32_t bytes);
#endif
