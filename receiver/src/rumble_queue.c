#include "rumble_queue.h"
#include <string.h>

void rumble_queue_reset(rumble_queue_t *q)
{
    memset(q, 0, sizeof(*q));
}

void rumble_queue_submit(rumble_queue_t *q, uint8_t left, uint8_t right)
{
    q->left = left;
    q->right = right;
    q->pending = true;
    if (left == 0 && right == 0) {
        q->stop_pending = true;
        /* Expedite a stop, but preserve backoff if the transport failed. */
        if (q->failures == 0) q->wait_ms = 0;
    }
}

bool rumble_queue_ready(const rumble_queue_t *q, uint32_t now)
{
    return q->pending && (uint32_t)(now - q->last_attempt_ms) >= q->wait_ms;
}

void rumble_queue_value(const rumble_queue_t *q, uint8_t *left, uint8_t *right)
{
    *left = q->stop_pending ? 0 : q->left;
    *right = q->stop_pending ? 0 : q->right;
}

void rumble_queue_result(rumble_queue_t *q, uint32_t now, bool success)
{
    q->last_attempt_ms = now;
    if (success) {
        q->pending = q->stop_pending && (q->left != 0 || q->right != 0);
        q->stop_pending = false;
        q->failures = 0;
        q->wait_ms = RUMBLE_INTERVAL_MS;
    } else {
        if (q->failures < RUMBLE_FAILURE_LIMIT) ++q->failures;
        q->wait_ms = (q->failures == 1) ? RUMBLE_INTERVAL_MS : q->wait_ms * 2;
        if (q->wait_ms > RUMBLE_BACKOFF_MAX_MS)
            q->wait_ms = RUMBLE_BACKOFF_MAX_MS;
    }
}
