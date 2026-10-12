#include "rumble_queue.h"
#include <stdio.h>

static int s_fail;
static void check(const char *name, bool ok)
{
    printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) s_fail = 1;
}

int main(void)
{
    rumble_queue_t q;
    uint8_t left, right;
    rumble_queue_reset(&q);
    check("idle does not send", !rumble_queue_ready(&q, 0));
    rumble_queue_submit(&q, 10, 20);
    check("first command is ready", rumble_queue_ready(&q, 0));
    rumble_queue_result(&q, 100, false);
    rumble_queue_submit(&q, 30, 40);
    rumble_queue_value(&q, &left, &right);
    check("new amplitude retains failures and backoff",
          q.failures == 1 && !rumble_queue_ready(&q, 109) &&
          rumble_queue_ready(&q, 110) && left == 30 && right == 40);

    /* A game streaming new amplitudes must still reach recovery threshold. */
    uint32_t now = 110;
    bool capped = true;
    for (unsigned i = 1; i < RUMBLE_FAILURE_LIMIT; ++i) {
        rumble_queue_submit(&q, (uint8_t)i, 255);
        rumble_queue_result(&q, now, false);
        if (q.wait_ms > RUMBLE_BACKOFF_MAX_MS) capped = false;
        now += q.wait_ms;
    }
    check("continuous commands cannot suppress failure recovery",
          q.failures == RUMBLE_FAILURE_LIMIT && q.wait_ms == 500 && capped);
    rumble_queue_result(&q, now, false);
    check("failure count saturates", q.failures == RUMBLE_FAILURE_LIMIT);

    rumble_queue_submit(&q, 0, 0);
    rumble_queue_submit(&q, 90, 80);
    rumble_queue_value(&q, &left, &right);
    check("stop cannot be overwritten by a later amplitude",
          left == 0 && right == 0 && !rumble_queue_ready(&q, now + 499));
    rumble_queue_result(&q, now + 500, true);
    rumble_queue_value(&q, &left, &right);
    check("after stop success the latest amplitude remains queued",
          q.failures == 0 && left == 90 && right == 80 &&
          !rumble_queue_ready(&q, now + 509) && rumble_queue_ready(&q, now + 510));
    rumble_queue_result(&q, now + 510, true);
    check("success empties queue", !q.pending);
    rumble_queue_submit(&q, 0, 0);
    check("healthy stop bypasses rate limit", rumble_queue_ready(&q, now + 511));
    rumble_queue_result(&q, now + 511, true);
    check("stop with no restart empties queue", !q.pending);

    rumble_queue_submit(&q, 1, 1);
    rumble_queue_result(&q, UINT32_MAX - 4, false);
    check("retry before and after timer wrap",
          !rumble_queue_ready(&q, 4) && rumble_queue_ready(&q, 5));
    rumble_queue_reset(&q);
    check("disconnect discards previous output and failure state",
          !q.pending && !q.stop_pending && q.failures == 0 && q.wait_ms == 0);
    printf("RESULT: %s\n", s_fail ? "FAIL" : "PASS");
    return s_fail;
}
