#ifndef DRIVERS_MOUSE_H
#define DRIVERS_MOUSE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mouse_state {
    int x;
    int y;
    bool left;
    bool right;
    bool middle;
    int8_t wheel_delta;
} mouse_state;

void mouse_init(uint32_t screen_w, uint32_t screen_h);
void mouse_set_bounds(uint32_t screen_w, uint32_t screen_h);
void mouse_set_sensitivity(uint8_t level);
bool mouse_ready(void);
bool mouse_poll(mouse_state* out);
bool mouse_get_state(mouse_state* out);

#ifdef __cplusplus
}
#endif

#endif
