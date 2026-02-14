#ifndef GUI_CURSOR_MANAGER_H
#define GUI_CURSOR_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum cursor_context {
    CURSOR_CONTEXT_DEFAULT = 0,
    CURSOR_CONTEXT_TEXT = 1,
    CURSOR_CONTEXT_CLICKABLE = 2,
    CURSOR_CONTEXT_RESIZE_EW = 3,
    CURSOR_CONTEXT_RESIZE_NS = 4,
    CURSOR_CONTEXT_RESIZE_NWSE = 5,
    CURSOR_CONTEXT_RESIZE_NESW = 6,
} cursor_context;

void cursor_manager_init(uint32_t screen_w, uint32_t screen_h);
void cursor_manager_set_scene(const uint32_t* scene_rgb, uint32_t scene_pitch_pixels);
void cursor_manager_set_position(int x, int y);
void cursor_manager_set_context(cursor_context context);
void cursor_manager_on_scene_redraw(void);
void cursor_manager_step(void);
bool cursor_manager_get_drawn_bounds(int* x, int* y, int* w, int* h);
bool cursor_manager_get_target_bounds(int* x, int* y, int* w, int* h);

#ifdef __cplusplus
}
#endif

#endif
