/* Uses the public-domain Bluepad32 ESP-IDF example entry-point pattern. */
#include <btstack_port_esp32.h>
#include <btstack_run_loop.h>
#include <uni.h>
#include "sdkconfig.h"
#if !defined(CONFIG_IDF_TARGET_ESP32) || !defined(CONFIG_BLUEPAD32_PLATFORM_CUSTOM)
#error "Use original ESP32 and Bluepad32 custom platform. S3/C3 do not support first-generation Pro Bluetooth."
#endif
struct uni_platform *switchpro_platform(void);
void app_main(void) {
    btstack_init();
    uni_platform_set_custom(switchpro_platform());
    uni_init(0, NULL);
    btstack_run_loop_execute();
}
