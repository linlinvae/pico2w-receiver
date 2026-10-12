#include "button_gesture.h"
#include <stdio.h>
static int failed;
static void check(const char *name, bool ok)
{ printf("%s %s\n", ok ? "PASS" : "FAIL", name); failed |= !ok; }
int main(void)
{
    button_gesture_t s = {0};
    check("idle does not emit a gesture", button_gesture_update(&s,false,50)==BUTTON_NONE);
    check("press waits to distinguish short and long", button_gesture_update(&s,true,100)==BUTTON_NONE);
    check("short press emits on release", button_gesture_update(&s,false,200)==BUTTON_SHORT);
    check("release emits once", button_gesture_update(&s,false,201)==BUTTON_NONE);
    button_gesture_update(&s,true,1000);
    check("long threshold is three seconds", button_gesture_update(&s,true,3999)==BUTTON_NONE);
    check("held button emits long once at threshold", button_gesture_update(&s,true,4000)==BUTTON_LONG);
    check("continued hold cannot repeat erase", button_gesture_update(&s,true,9000)==BUTTON_NONE);
    check("long release cannot trigger short scan", button_gesture_update(&s,false,9100)==BUTTON_NONE);
    button_gesture_update(&s,true,10000);
    check("late polling on release still recognizes long", button_gesture_update(&s,false,13000)==BUTTON_LONG);
    button_gesture_update(&s,true,UINT32_MAX-1000);
    check("long press across counter wrap waits full duration", button_gesture_update(&s,true,1998)==BUTTON_NONE);
    check("long press across counter wrap fires at threshold", button_gesture_update(&s,true,1999)==BUTTON_LONG);
    button_gesture_update(&s,false,2000);
    button_gesture_update(&s,true,2010);
    check("short press still works after a long press", button_gesture_update(&s,false,2100)==BUTTON_SHORT);
    return failed;
}
