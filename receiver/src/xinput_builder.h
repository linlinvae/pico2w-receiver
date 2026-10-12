#ifndef XINPUT_BUILDER_H
#define XINPUT_BUILDER_H
#include <stdint.h>
#include <stdbool.h>
#define XINPUT_REPORT_BYTES 20
/* Wired XUSB 1.00 report. Uses the existing Stadia/JP button bit map. */
void xinput_build_report(uint32_t buttons, const uint8_t analog[6], uint8_t out[20]);
void xinput_build_neutral_report(uint8_t out[20]);
bool xinput_parse_rumble(const uint8_t *data, uint16_t len, uint8_t *left, uint8_t *right);
#endif
