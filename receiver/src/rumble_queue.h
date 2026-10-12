#ifndef RUMBLE_QUEUE_H
#define RUMBLE_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#define RUMBLE_FAILURE_LIMIT 50
#define RUMBLE_INTERVAL_MS 10
#define RUMBLE_BACKOFF_MAX_MS 500

/* One latest desired value and a stop latch; no dynamic allocation. */
typedef struct {
    uint32_t last_attempt_ms;
    uint16_t wait_ms;
    uint8_t left, right, failures;
    bool pending, stop_pending;
} rumble_queue_t;

void rumble_queue_reset(rumble_queue_t *q);
void rumble_queue_submit(rumble_queue_t *q, uint8_t left, uint8_t right);
bool rumble_queue_ready(const rumble_queue_t *q, uint32_t now);
void rumble_queue_value(const rumble_queue_t *q, uint8_t *left, uint8_t *right);
void rumble_queue_result(rumble_queue_t *q, uint32_t now, bool success);

#endif
