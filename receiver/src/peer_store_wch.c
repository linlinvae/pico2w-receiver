#ifdef CFG_CH582
#include "CH58x_common.h"
#else
#include "CH59x_common.h"
#endif
#include "CONFIG.h"
#include "board_config.h"
#include "peer_store.h"
#if BOARD_PEER_STORE_ADDR % EEPROM_MIN_ER_SIZE != 0
#error Peer journal must be erase-page aligned
#endif
#if BOARD_PEER_STORE_ADDR + 2 * PEER_STORE_PAGE_BYTES > EEPROM_MAX_SIZE
#error Peer journal exceeds DataFlash
#endif
#if !(BOARD_PEER_STORE_ADDR + 2 * PEER_STORE_PAGE_BYTES <= BLE_SNV_ADDR || BOARD_PEER_STORE_ADDR >= BLE_SNV_ADDR + BLE_SNV_BLOCK * BLE_SNV_NUM)
#error Peer journal overlaps WCH BLE SNV
#endif
uint32_t peer_flash_read(uint32_t address, void *data, uint32_t bytes)
{ return EEPROM_READ(address,data,bytes); }
uint32_t peer_flash_erase(uint32_t address, uint32_t bytes)
{ return EEPROM_ERASE(address,bytes); }
uint32_t peer_flash_write(uint32_t address, const void *data, uint32_t bytes)
{ return EEPROM_WRITE(address,(void *)data,bytes); }
