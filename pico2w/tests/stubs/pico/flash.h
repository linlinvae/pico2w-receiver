#include <stdint.h>
int flash_safe_execute(void (*fn)(void *),void *arg,uint32_t timeout);
