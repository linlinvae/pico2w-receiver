#include "button_gesture.h"
button_gesture_event_t button_gesture_update(button_gesture_t *s, bool pressed, uint32_t now)
{
    if (pressed && !s->pressed) {
        s->pressed=true; s->long_sent=false; s->pressed_at=now;
    }
    if (s->pressed && !s->long_sent && now-s->pressed_at>=3000) {
        s->long_sent=true;
        s->pressed=pressed;
        return BUTTON_LONG;
    }
    if (!pressed && s->pressed) {
        s->pressed=false;
        if (!s->long_sent) return BUTTON_SHORT;
    }
    return BUTTON_NONE;
}
