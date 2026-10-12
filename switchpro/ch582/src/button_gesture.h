#ifndef BUTTON_GESTURE_H
#define BUTTON_GESTURE_H
#include <stdint.h>
#include <stdbool.h>
typedef enum { BUTTON_NONE, BUTTON_SHORT, BUTTON_LONG } button_gesture_event_t;
typedef struct { uint32_t pressed_at; bool pressed, long_sent; } button_gesture_t;
/* Input is already debounced. Short press fires on release; long press once. */
button_gesture_event_t button_gesture_update(button_gesture_t *s, bool pressed, uint32_t now);
#endif
