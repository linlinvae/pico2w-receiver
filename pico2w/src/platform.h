#ifndef RECEIVER_PLATFORM_H
#define RECEIVER_PLATFORM_H
#include <stdint.h>
struct uni_hid_device_s;
struct uni_platform *receiver_platform(void);
/* Observe packet arrival, including unchanged buttons; Switch 0x30 is continuous. */
void receiver_switch_report_received(struct uni_hid_device_s *device, const uint8_t *report, uint16_t len);
#endif
