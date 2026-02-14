#ifndef GUI_DESKTOP_H
#define GUI_DESKTOP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "kernel/cli.h"

#ifdef __cplusplus
extern "C" {
#endif

void desktop_init(void);
void desktop_tick(void);
void desktop_queue_key(char c);
bool desktop_consume_kernel_action(cli_action* out_action);
void desktop_append_log(const char* line);
void desktop_clear_log(void);
void desktop_force_redraw(void);
void desktop_set_mouse(int x, int y, bool left_down, bool right_down, bool middle_down, int8_t wheel_delta);
bool desktop_open_app_by_name(const char* name);
uint32_t desktop_uptime_seconds(void);
void desktop_report_idle_spins(uint32_t idle_spins);
void desktop_enter_sleep_mode(void);
void desktop_logout_session(void);
uint8_t desktop_theme_index(void);
bool desktop_set_theme_index(uint8_t theme_idx);
uint8_t desktop_resolution_mode(void);
void desktop_toggle_resolution_mode(void);
uint8_t desktop_mouse_speed(void);
bool desktop_set_mouse_speed(uint8_t speed);
const char* desktop_current_user(void);

#ifdef __cplusplus
}
#endif

#endif
