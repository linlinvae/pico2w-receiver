#ifndef SP_LINK_H
#define SP_LINK_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SP_FRAME_SIZE 24
#define SP_PAYLOAD_SIZE 16
#define SP_LINK_TIMEOUT_MS 250
#define SP_COMMAND_INTERVAL_MS 100
#define SP_COMMAND_WATCHDOG_MS 300
enum { SP_INPUT = 1, SP_COMMAND = 2, SP_ACK = 3 };
enum { SP_OP_NONE = 0, SP_OP_SCAN = 1, SP_OP_FORGET = 2 };
typedef struct { uint8_t type; uint16_t seq; uint8_t data[16]; } sp_frame_t;
typedef struct { uint8_t bytes[24]; uint8_t used; } sp_decoder_t;
typedef struct {
    bool connected;
    uint8_t battery;
    uint16_t buttons;
    int16_t axis[4]; /* XInput coordinates: Y positive up. */
    uint8_t lt, rt;
} sp_input_t;

uint16_t sp_crc16(const uint8_t *data, size_t size);
void sp_encode(const sp_frame_t *frame, uint8_t wire[24]);
bool sp_decode_byte(sp_decoder_t *decoder, uint8_t byte, sp_frame_t *frame);
void sp_pack_input(const sp_input_t *input, uint8_t payload[16]);
bool sp_unpack_input(const uint8_t payload[16], sp_input_t *input);
void sp_xinput_report(const sp_input_t *input, uint8_t report[20]);
int16_t sp_scale_axis(int32_t value, bool invert);
void sp_map_gamepad(uint16_t buttons, uint8_t dpad, uint8_t misc,
                    const int32_t axis[4], uint8_t battery, bool swap_labels,
                    sp_input_t *input);
#endif
