#include "gui/desktop.h"

#include "drivers/framebuffer.h"
#include "drivers/mouse.h"
#include "gui/cursor_manager.h"
#include "gui/font5x7.h"
#include "gui/image_loader.h"
#include "kernel/console.h"
#include "kernel/filesystem.h"
#include "kernel/net_stack.h"
#include "kernel/release.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    kScreenWidth = 1280,
    kScreenHeight = 720,
    kBackbufferMaxW = kScreenWidth,
    kBackbufferMaxH = kScreenHeight,
    kLogLines = 256,
    kLogLineLen = 160,
    kLogWrapChars = 140,
    kTicksPerSecondEstimate = 60U,
    kCursorBlinkFrames = 28,
    kTerminalCellW = 8,
    kTerminalCellH = 16,
    kTerminalGlyphOffsetX = 1,
    kTerminalGlyphOffsetY = 4,
    kTaskbarH = 34,
    kTitlebarH = 20,
    kTitleBarButtonSize = 16,
    kTitleBarButtonPadding = 4,
    kInputH = 28,
    kStatusH = 24,
    kStartMenuHeaderH = 44,
    kStartMenuItemH = 13,
    kStartMenuItems = 33,
    kDesktopIconCount = 32,
    kDesktopIconSize = 42,
    kDesktopIconLabelW = 110,
    kDesktopIconCols = 8,
    kDesktopIconCellW = 120,
    kDesktopIconCellH = 86,
    kDesktopIconTopPad = 12,
    kDesktopIconBottomPad = 14,
    kAppWindowCount = 32,
    kAppWindowW = 500,
    kAppWindowH = 320,
    kAppWindowTitleH = 20,
    kQuickLaunchW = 56,
    kQuickLaunchGap = 4,
    kAppTaskButtonW = 76,
    kAppTaskButtonGap = 3,
    kWindowMinW = 320,
    kWindowMinH = 220,
    kResizeEdgeTolerance = 3,
    kResizeLeft = 1,
    kResizeRight = 2,
    kResizeTop = 4,
    kResizeBottom = 8,
    kThemeCount = 1,
    kPerfHistory = 64,
    kNotesMax = 1536,
    kEditorMax = 2048,
    kFileRowH = 14,
    kFileRowsVisible = 10,
    kBootAnimFrames = 120,
    kLoginPinMax = 12,
    kKeyQueueCap = 128,
};

typedef enum app_id {
    APP_HELP = 0,
    APP_FILES = 1,
    APP_SYSTEM = 2,
    APP_MOUSE = 3,
    APP_SETTINGS = 4,
    APP_PERFORMANCE = 5,
    APP_NOTES = 6,
    APP_ABOUT = 7,
    APP_CREDITS = 8,
    APP_TIPS = 9,
    APP_DOOM = 10,
    APP_EDITOR = 11,
    APP_CALCULATOR = 12,
    APP_CLOCK = 13,
    APP_CALENDAR = 14,
    APP_TASKS = 15,
    APP_CLIPBOARD = 16,
    APP_NETWORK = 17,
    APP_STORAGE = 18,
    APP_DIAGNOSTICS = 19,
    APP_MONITOR = 20,
    APP_TERMINAL_GUIDE = 21,
    APP_WALLPAPER = 22,
    APP_SHORTCUTS = 23,
    APP_TROUBLESHOOT = 24,
    APP_RELEASE_NOTES = 25,
    APP_ROADMAP = 26,
    APP_JOURNAL = 27,
    APP_TODO = 28,
    APP_PACKAGES = 29,
    APP_SNAPSHOTS = 30,
    APP_LAUNCHER = 31,
} app_id;

typedef enum session_user {
    SESSION_USER_NONE = 0,
    SESSION_USER_ROOT,
    SESSION_USER_GUEST,
} session_user;

typedef struct ui_palette {
    uint32_t desktop_bg;
    uint32_t desktop_line;

    uint32_t frame_bg;
    uint32_t frame_light;
    uint32_t frame_dark;
    uint32_t frame_darker;

    uint32_t title_top;
    uint32_t title_bottom;
    uint32_t title_text;
    uint32_t title_subtext;

    uint32_t text_primary;
    uint32_t text_muted;
    uint32_t log_bg;
    uint32_t input_bg;
    uint32_t status_bg;

    uint32_t taskbar_bg;
    uint32_t start_bg;
    uint32_t start_menu_bg;
    uint32_t menu_hover_bg;
    uint32_t menu_hover_text;
} ui_palette;

typedef struct rect_i {
    int x;
    int y;
    int w;
    int h;
} rect_i;

typedef enum wm_message {
    WM_NONE = 0,
    WM_CLOSE,
    WM_MINIMIZE,
    WM_MAXIMIZE,
    WM_RESTORE,
} wm_message;

typedef struct Window {
    int x, y;
    int width, height;
    bool is_minimized;
    bool is_maximized;
    bool is_active;
    int prev_x, prev_y;
    int prev_width, prev_height;
} Window;

typedef struct wm_window {
    int x;
    int y;
    int w;
    int h;
    int restore_x;
    int restore_y;
    int restore_w;
    int restore_h;
    bool minimized;
    bool maximized;
    bool dragging;
    bool resizing;
    uint8_t resize_edges;
    int drag_dx;
    int drag_dy;
    int resize_anchor_mouse_x;
    int resize_anchor_mouse_y;
    int resize_anchor_x;
    int resize_anchor_y;
    int resize_anchor_w;
    int resize_anchor_h;
} wm_window;

typedef struct app_window {
    int x;
    int y;
    int w;
    int h;
    bool open;
    bool minimized;
    bool maximized;
    int prev_x;
    int prev_y;
    int prev_w;
    int prev_h;
} app_window;

typedef struct ui_layout {
    int screen_w;
    int screen_h;
    rect_i taskbar;
    rect_i start_button;
    rect_i task_terminal_button;
    rect_i quick_help_button;
    rect_i quick_files_button;
    rect_i quick_doom_button;
    rect_i clock_box;
    rect_i start_menu;
    rect_i window;
    rect_i titlebar;
    rect_i btn_min;
    rect_i btn_max;
    rect_i btn_close;
    rect_i log_box;
    rect_i input_box;
    rect_i status_box;
} ui_layout;

static const ui_palette kPalette = {
    .desktop_bg = 0x0F1724,
    .desktop_line = 0x182133,

    .frame_bg = 0x1B2735,
    .frame_light = 0x273549,
    .frame_dark = 0x0F1822,
    .frame_darker = 0x0A0F16,

    .title_top = 0x1F3A63,
    .title_bottom = 0x15253D,
    .title_text = 0xEAF2FF,
    .title_subtext = 0xA5B7D1,

    .text_primary = 0xE8EDF5,
    .text_muted = 0x9BB1C7,
    .log_bg = 0x111A27,
    .input_bg = 0x111A27,
    .status_bg = 0x111A27,

    .taskbar_bg = 0x0F1724,
    .start_bg = 0x1F4F7A,
    .start_menu_bg = 0x111A27,
    .menu_hover_bg = 0x2A6FC4,
    .menu_hover_text = 0xFFFFFF,
};

static const char* kStartMenuLabels[kStartMenuItems] = {
    "Open Help",
    "File Explorer",
    "System Info",
    "Mouse Panel",
    "Settings",
    "Performance",
    "Notes",
    "About PyCoreOS",
    "Lead Credits",
    "Desktop Tips",
    "Run Doom",
    "Text Editor",
    "Calculator",
    "Clock",
    "Calendar",
    "Tasks",
    "Clipboard",
    "Network",
    "Storage",
    "Diagnostics",
    "Resource Monitor",
    "Terminal Guide",
    "Wallpaper",
    "Shortcuts",
    "Troubleshoot",
    "Release Notes",
    "Roadmap",
    "Journal",
    "Todo",
    "Packages",
    "Snapshots",
    "Quick Launcher",
    "Clear Terminal",
};

static const int kStartMenuActions[kStartMenuItems] = {
    APP_HELP,
    APP_FILES,
    APP_SYSTEM,
    APP_MOUSE,
    APP_SETTINGS,
    APP_PERFORMANCE,
    APP_NOTES,
    APP_ABOUT,
    APP_CREDITS,
    APP_TIPS,
    APP_DOOM,
    APP_EDITOR,
    APP_CALCULATOR,
    APP_CLOCK,
    APP_CALENDAR,
    APP_TASKS,
    APP_CLIPBOARD,
    APP_NETWORK,
    APP_STORAGE,
    APP_DIAGNOSTICS,
    APP_MONITOR,
    APP_TERMINAL_GUIDE,
    APP_WALLPAPER,
    APP_SHORTCUTS,
    APP_TROUBLESHOOT,
    APP_RELEASE_NOTES,
    APP_ROADMAP,
    APP_JOURNAL,
    APP_TODO,
    APP_PACKAGES,
    APP_SNAPSHOTS,
    APP_LAUNCHER,
    -1,
};

static const char* kDesktopIconLabels[kDesktopIconCount] = {
    "HELP",
    "FILES",
    "SYS",
    "MOUSE",
    "SET",
    "PERF",
    "NOTES",
    "ABOUT",
    "CRED",
    "TIPS",
    "DOOM",
    "EDIT",
    "CALC",
    "CLOCK",
    "CAL",
    "TASK",
    "CLIP",
    "NET",
    "DISK",
    "DIAG",
    "MON",
    "GUIDE",
    "WALL",
    "SHORT",
    "FIX",
    "REL",
    "ROAD",
    "JRNL",
    "TODO",
    "PKG",
    "SNAP",
    "LAUNCH",
};

static const app_id kDesktopIconApps[kDesktopIconCount] = {
    APP_HELP,
    APP_FILES,
    APP_SYSTEM,
    APP_MOUSE,
    APP_SETTINGS,
    APP_PERFORMANCE,
    APP_NOTES,
    APP_ABOUT,
    APP_CREDITS,
    APP_TIPS,
    APP_DOOM,
    APP_EDITOR,
    APP_CALCULATOR,
    APP_CLOCK,
    APP_CALENDAR,
    APP_TASKS,
    APP_CLIPBOARD,
    APP_NETWORK,
    APP_STORAGE,
    APP_DIAGNOSTICS,
    APP_MONITOR,
    APP_TERMINAL_GUIDE,
    APP_WALLPAPER,
    APP_SHORTCUTS,
    APP_TROUBLESHOOT,
    APP_RELEASE_NOTES,
    APP_ROADMAP,
    APP_JOURNAL,
    APP_TODO,
    APP_PACKAGES,
    APP_SNAPSHOTS,
    APP_LAUNCHER,
};

static const char* kAppWindowTitles[kAppWindowCount] = {
    "HELP CENTER",
    "FILE EXPLORER",
    "SYSTEM INFO",
    "MOUSE PANEL",
    "SETTINGS",
    "PERFORMANCE",
    "NOTES",
    "ABOUT PYCOREOS",
    "LEAD CREDITS",
    "DESKTOP TIPS",
    "DOOM LAUNCHER",
    "TEXT EDITOR",
    "CALCULATOR",
    "CLOCK",
    "CALENDAR",
    "TASKS",
    "CLIPBOARD",
    "NETWORK",
    "STORAGE",
    "DIAGNOSTICS",
    "RESOURCE MONITOR",
    "TERMINAL GUIDE",
    "WALLPAPER",
    "SHORTCUTS",
    "TROUBLESHOOT",
    "RELEASE NOTES",
    "ROADMAP",
    "JOURNAL",
    "TODO",
    "PACKAGE CENTER",
    "SNAPSHOTS",
    "QUICK LAUNCHER",
};

static uint32_t s_ticks = 0;
static uint32_t s_last_frame_tick = 0;
static bool s_needs_redraw = true;

static char s_input_line[64];
static size_t s_input_len = 0;

static char s_pending_command[64];
static bool s_has_pending_command = false;
static char s_key_queue[kKeyQueueCap];
static size_t s_key_queue_head = 0;
static size_t s_key_queue_tail = 0;
static cli_action s_pending_kernel_action = CLI_ACTION_NONE;

static char s_log[kLogLines][kLogLineLen];
static size_t s_log_count = 0;

static bool s_graphics = false;
static uint32_t s_backbuffer[kBackbufferMaxW * kBackbufferMaxH];
static uint32_t s_static_cache[kBackbufferMaxW * kBackbufferMaxH];
static bool s_static_cache_valid = false;

static uint32_t* s_draw_target = s_backbuffer;
static rect_i s_clip_rect = {0, 0, kScreenWidth, kScreenHeight};
static bool s_clip_enabled = false;
static rect_i s_dirty_rect = {0, 0, kScreenWidth, kScreenHeight};
static bool s_dirty_valid = true;
static uint32_t s_autosave_ticks = 0;

static wm_window s_terminal_window;
static bool s_start_menu_open = false;
static app_window s_app_windows[kAppWindowCount];
static int s_app_z_order[kAppWindowCount];
static int s_drag_app_idx = -1;
static int s_drag_app_dx = 0;
static int s_drag_app_dy = 0;
static int s_resize_app_idx = -1;
static int s_resize_app_anchor_w = 0;
static int s_resize_app_anchor_h = 0;
static int s_resize_app_anchor_mouse_x = 0;
static int s_resize_app_anchor_mouse_y = 0;

static bool s_active_is_terminal = true;
static int s_active_app_idx = -1;

static int s_terminal_btn_pressed = 0;
static int s_app_btn_pressed_idx = -1;
static int s_app_btn_pressed_btn = 0;

static rect_i s_icon_cells[kDesktopIconCount];
static bool s_icons_initialized = false;
static int s_icon_press_idx = -1;
static int s_icon_drag_idx = -1;
static int s_icon_drag_dx = 0;
static int s_icon_drag_dy = 0;
static int s_icon_press_x = 0;
static int s_icon_press_y = 0;
static bool s_icon_drag_moved = false;

static char s_start_search[24];
static size_t s_start_search_len = 0;
static bool s_start_search_focused = false;
static bool s_sleeping = false;
static uint32_t s_boot_anim_tick = 0;
static session_user s_session_user = SESSION_USER_NONE;
static bool s_login_guest_selected = false;
static char s_login_pin[kLoginPinMax + 1];
static size_t s_login_pin_len = 0;
static char s_login_message[80];

static rect_i s_last_tooltip_rect = {0, 0, 0, 0};
static bool s_last_tooltip_visible = false;

static int s_mouse_x = 0;
static int s_mouse_y = 0;
static bool s_mouse_left = false;
static bool s_mouse_right = false;
static bool s_mouse_middle = false;
static int s_mouse_wheel_accum = 0;
static int s_pending_mouse_x = 0;
static int s_pending_mouse_y = 0;
static bool s_pending_mouse_left = false;
static bool s_pending_mouse_right = false;
static bool s_pending_mouse_middle = false;
static int s_pending_mouse_wheel = 0;
static bool s_mouse_pending = false;
static cursor_context s_cursor_context = CURSOR_CONTEXT_DEFAULT;
static bool s_font_profile_16_10_1680x1050 = false;
static int s_log_scroll = 0;
static uint32_t s_blink_frame_counter = 0;
static bool s_input_cursor_visible = true;

static int s_theme_index = 0;
static uint32_t s_theme_desktop_bg = 0x0F1724;
static uint32_t s_theme_desktop_line = 0x182133;
static uint32_t s_theme_taskbar_bg = 0x0F1724;
static uint32_t s_theme_start_bg = 0x1F4F7A;
static uint32_t s_theme_menu_bg = 0x111A27;
static uint32_t s_theme_menu_hover_bg = 0x2A6FC4;
static uint32_t s_theme_menu_hover_text = 0xFFFFFF;

static bool s_wallpaper_loaded = false;
static uint32_t s_wallpaper[kBackbufferMaxW * kBackbufferMaxH];

static uint8_t s_setting_mouse_speed = 2;
static int s_settings_resolution_mode = 0;

static int s_files_selected = -1;

static uint32_t s_last_idle_spins = 0;
static uint32_t s_max_idle_spins = 1;
static uint8_t s_cpu_history[kPerfHistory];
static uint8_t s_mem_history[kPerfHistory];
static int s_perf_hist_len = 0;
static int s_perf_hist_head = 0;

static bool s_notes_focused = false;
static bool s_notes_dirty = false;
static char s_notes_text[kNotesMax];
static size_t s_notes_len = 0;
static size_t s_notes_cursor = 0;

static bool s_editor_focused = false;
static bool s_editor_dirty = false;
static char s_editor_filename[48];
static char s_editor_text[kEditorMax];
static size_t s_editor_len = 0;
static size_t s_editor_cursor = 0;

static char s_calc_display[32];
static int s_calc_accum = 0;
static char s_calc_op = 0;
static bool s_calc_new_entry = true;

static void compute_layout(ui_layout* out);
static void start_menu_reset_search(void);
static void process_queued_keys(void);
static void process_pending_shell_command(void);

static void copy_str(char* dst, size_t cap, const char* src) {
    if (cap == 0) {
        return;
    }

    size_t i = 0;
    while (src[i] != '\0' && i + 1 < cap) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static size_t cstr_len(const char* s) {
    size_t n = 0;
    while (s[n] != '\0') {
        ++n;
    }
    return n;
}

static bool cstr_eq(const char* a, const char* b) {
    size_t i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return false;
        }
        ++i;
    }
    return a[i] == '\0' && b[i] == '\0';
}

static bool key_queue_push(char c) {
    const size_t next = (s_key_queue_head + 1U) % kKeyQueueCap;
    if (next == s_key_queue_tail) {
        return false;
    }
    s_key_queue[s_key_queue_head] = c;
    s_key_queue_head = next;
    return true;
}

static bool key_queue_pop(char* out) {
    if (out == NULL || s_key_queue_head == s_key_queue_tail) {
        return false;
    }
    *out = s_key_queue[s_key_queue_tail];
    s_key_queue_tail = (s_key_queue_tail + 1U) % kKeyQueueCap;
    return true;
}

static char to_lower_ascii(char c) {
    if (c >= 'A' && c <= 'Z') {
        return (char)(c - 'A' + 'a');
    }
    return c;
}

static bool cstr_contains_icase(const char* haystack, const char* needle) {
    if (haystack == NULL || needle == NULL) {
        return false;
    }
    if (needle[0] == '\0') {
        return true;
    }

    for (size_t i = 0; haystack[i] != '\0'; ++i) {
        size_t j = 0;
        while (needle[j] != '\0' &&
               haystack[i + j] != '\0' &&
               to_lower_ascii(haystack[i + j]) == to_lower_ascii(needle[j])) {
            ++j;
        }
        if (needle[j] == '\0') {
            return true;
        }
    }
    return false;
}

static int clamp_i32(int value, int low, int high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

static uint32_t clamp_u32(uint32_t value, uint32_t low, uint32_t high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

static uint32_t color_lerp(uint32_t a, uint32_t b, uint32_t t255) {
    const int32_t ar = (int32_t)((a >> 16U) & 0xFFU);
    const int32_t ag = (int32_t)((a >> 8U) & 0xFFU);
    const int32_t ab = (int32_t)(a & 0xFFU);
    const int32_t br = (int32_t)((b >> 16U) & 0xFFU);
    const int32_t bg = (int32_t)((b >> 8U) & 0xFFU);
    const int32_t bb = (int32_t)(b & 0xFFU);

    const uint32_t r = (uint32_t)(ar + ((br - ar) * (int32_t)t255) / 255);
    const uint32_t g = (uint32_t)(ag + ((bg - ag) * (int32_t)t255) / 255);
    const uint32_t bch = (uint32_t)(ab + ((bb - ab) * (int32_t)t255) / 255);
    return (r << 16U) | (g << 8U) | bch;
}

static void buf_append_char(char* out, size_t cap, size_t* idx, char c) {
    if (*idx + 1 >= cap) {
        return;
    }
    out[*idx] = c;
    ++(*idx);
    out[*idx] = '\0';
}

static void buf_append_str(char* out, size_t cap, size_t* idx, const char* text) {
    size_t i = 0;
    while (text[i] != '\0') {
        buf_append_char(out, cap, idx, text[i]);
        ++i;
    }
}

static void buf_append_u32(char* out, size_t cap, size_t* idx, uint32_t value) {
    char tmp[16];
    size_t n = 0;

    if (value == 0) {
        buf_append_char(out, cap, idx, '0');
        return;
    }

    while (value > 0 && n < sizeof(tmp)) {
        tmp[n++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while (n > 0) {
        buf_append_char(out, cap, idx, tmp[--n]);
    }
}

static void format_seconds_hms(uint32_t seconds, char* out, size_t cap) {
    if (cap < 9) {
        if (cap > 0) out[0] = '\0';
        return;
    }
    const uint32_t h = seconds / 3600U;
    const uint32_t m = (seconds / 60U) % 60U;
    const uint32_t s = seconds % 60U;
    out[0] = (char)('0' + (h / 10U) % 10U);
    out[1] = (char)('0' + (h % 10U));
    out[2] = ':';
    out[3] = (char)('0' + (m / 10U));
    out[4] = (char)('0' + (m % 10U));
    out[5] = ':';
    out[6] = (char)('0' + (s / 10U));
    out[7] = (char)('0' + (s % 10U));
    out[8] = '\0';
}

static bool cstr_starts_with(const char* text, const char* prefix) {
    size_t i = 0;
    while (prefix[i] != '\0') {
        if (text[i] != prefix[i]) {
            return false;
        }
        ++i;
    }
    return true;
}

static bool cstr_ends_with(const char* text, const char* suffix) {
    const size_t text_len = cstr_len(text);
    const size_t suffix_len = cstr_len(suffix);
    if (suffix_len > text_len) {
        return false;
    }
    const size_t offset = text_len - suffix_len;
    for (size_t i = 0; i < suffix_len; ++i) {
        if (text[offset + i] != suffix[i]) {
            return false;
        }
    }
    return true;
}

static bool parse_u32(const char* s, uint32_t* out) {
    if (s == NULL || out == NULL || *s == '\0') {
        return false;
    }
    uint32_t value = 0;
    for (size_t i = 0; s[i] != '\0'; ++i) {
        if (s[i] < '0' || s[i] > '9') {
            return false;
        }
        value = value * 10U + (uint32_t)(s[i] - '0');
    }
    *out = value;
    return true;
}

static void apply_theme(int theme_idx) {
    int idx = theme_idx;
    (void)idx;
    s_theme_index = 0;
    s_theme_desktop_bg = 0x0F1724;
    s_theme_desktop_line = 0x182133;
    s_theme_taskbar_bg = 0x0F1724;
    s_theme_start_bg = 0x1F4F7A;
    s_theme_menu_bg = 0x111A27;
    s_theme_menu_hover_bg = 0x2A6FC4;
    s_theme_menu_hover_text = 0xFFFFFF;
}

static void settings_save(void) {
    char cfg[160];
    size_t idx = 0;
    cfg[0] = '\0';
    buf_append_str(cfg, sizeof(cfg), &idx, "mouse_speed=");
    buf_append_u32(cfg, sizeof(cfg), &idx, (uint32_t)s_setting_mouse_speed);
    buf_append_char(cfg, sizeof(cfg), &idx, '\n');
    buf_append_str(cfg, sizeof(cfg), &idx, "theme=");
    buf_append_u32(cfg, sizeof(cfg), &idx, (uint32_t)s_theme_index);
    buf_append_char(cfg, sizeof(cfg), &idx, '\n');
    buf_append_str(cfg, sizeof(cfg), &idx, "resolution_mode=");
    buf_append_u32(cfg, sizeof(cfg), &idx, (uint32_t)s_settings_resolution_mode);
    buf_append_char(cfg, sizeof(cfg), &idx, '\n');
    (void)fs_write("settings.cfg", cfg);
}

static void settings_load(void) {
    char cfg[220];
    if (!fs_read("settings.cfg", cfg, sizeof(cfg))) {
        return;
    }

    size_t i = 0;
    while (cfg[i] != '\0') {
        char line[80];
        size_t n = 0;
        while (cfg[i] != '\0' && cfg[i] != '\n' && n + 1 < sizeof(line)) {
            line[n++] = cfg[i++];
        }
        line[n] = '\0';
        if (cfg[i] == '\n') {
            ++i;
        }

        if (cstr_starts_with(line, "mouse_speed=")) {
            uint32_t v = 0;
            if (parse_u32(line + 12, &v) && v >= 1U && v <= 4U) {
                s_setting_mouse_speed = (uint8_t)v;
            }
            continue;
        }
        if (cstr_starts_with(line, "theme=")) {
            uint32_t v = 0;
            if (parse_u32(line + 6, &v) && v < (uint32_t)kThemeCount) {
                apply_theme((int)v);
            }
            continue;
        }
        if (cstr_starts_with(line, "resolution_mode=")) {
            uint32_t v = 0;
            if (parse_u32(line + 16, &v)) {
                s_settings_resolution_mode = (v == 0U) ? 0 : 1;
                s_font_profile_16_10_1680x1050 = (s_settings_resolution_mode != 0);
            }
            continue;
        }
    }

    mouse_set_sensitivity(s_setting_mouse_speed);
}

static void notes_set_text(const char* text) {
    if (text == NULL) {
        s_notes_text[0] = '\0';
        s_notes_len = 0;
        s_notes_cursor = 0;
        return;
    }

    size_t i = 0;
    while (text[i] != '\0' && i + 1 < sizeof(s_notes_text)) {
        s_notes_text[i] = text[i];
        ++i;
    }
    s_notes_text[i] = '\0';
    s_notes_len = i;
    if (s_notes_cursor > s_notes_len) {
        s_notes_cursor = s_notes_len;
    }
}

static void notes_load(void) {
    char buf[kNotesMax];
    if (!fs_read("notes.txt", buf, sizeof(buf))) {
        notes_set_text("");
        return;
    }
    notes_set_text(buf);
    s_notes_dirty = false;
}

static void notes_save(void) {
    (void)fs_write_bytes("notes.txt", s_notes_text, s_notes_len);
    s_notes_dirty = false;
}

static void perf_push_sample(uint8_t cpu_pct, uint8_t mem_pct) {
    s_cpu_history[s_perf_hist_head] = cpu_pct;
    s_mem_history[s_perf_hist_head] = mem_pct;
    s_perf_hist_head = (s_perf_hist_head + 1) % kPerfHistory;
    if (s_perf_hist_len < kPerfHistory) {
        ++s_perf_hist_len;
    }
}

static void wallpaper_load_from_fs(void) {
    s_wallpaper_loaded = false;

    const uint8_t* data = NULL;
    size_t size = 0;
    if (!fs_map_readonly("wallpaper.bmp", &data, &size) &&
        !fs_map_readonly("wallpaper.tga", &data, &size)) {
        return;
    }

    if (image_loader_decode_bmp_or_tga(data, size, s_wallpaper, kScreenWidth, kScreenHeight)) {
        s_wallpaper_loaded = true;
    }
}

static rect_i rect_make(int x, int y, int w, int h) {
    rect_i r;
    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;
    return r;
}

static bool rect_valid(rect_i r) {
    return r.w > 0 && r.h > 0;
}

static bool rect_contains(rect_i r, int px, int py) {
    if (!rect_valid(r)) {
        return false;
    }
    return px >= r.x && py >= r.y && px < (r.x + r.w) && py < (r.y + r.h);
}

static rect_i rect_intersect(rect_i a, rect_i b) {
    const int x0 = (a.x > b.x) ? a.x : b.x;
    const int y0 = (a.y > b.y) ? a.y : b.y;
    const int x1 = ((a.x + a.w) < (b.x + b.w)) ? (a.x + a.w) : (b.x + b.w);
    const int y1 = ((a.y + a.h) < (b.y + b.h)) ? (a.y + a.h) : (b.y + b.h);
    if (x1 <= x0 || y1 <= y0) {
        return rect_make(0, 0, 0, 0);
    }
    return rect_make(x0, y0, x1 - x0, y1 - y0);
}

static rect_i rect_union(rect_i a, rect_i b) {
    if (!rect_valid(a)) {
        return b;
    }
    if (!rect_valid(b)) {
        return a;
    }

    const int x0 = (a.x < b.x) ? a.x : b.x;
    const int y0 = (a.y < b.y) ? a.y : b.y;
    const int x1 = ((a.x + a.w) > (b.x + b.w)) ? (a.x + a.w) : (b.x + b.w);
    const int y1 = ((a.y + a.h) > (b.y + b.h)) ? (a.y + a.h) : (b.y + b.h);
    return rect_make(x0, y0, x1 - x0, y1 - y0);
}

static void request_redraw_rect(int x, int y, int w, int h) {
    rect_i r = rect_make(x, y, w, h);
    rect_i screen = rect_make(0, 0, kScreenWidth, kScreenHeight);
    r = rect_intersect(r, screen);
    if (!rect_valid(r)) {
        return;
    }

    if (s_dirty_valid) {
        s_dirty_rect = rect_union(s_dirty_rect, r);
    } else {
        s_dirty_rect = r;
        s_dirty_valid = true;
    }
    s_needs_redraw = true;
}

static void request_redraw(void) {
    s_static_cache_valid = false;
    request_redraw_rect(0, 0, kScreenWidth, kScreenHeight);
}

static void request_redraw_clock(void) {
    ui_layout l;
    compute_layout(&l);
    request_redraw_rect(l.clock_box.x, l.clock_box.y, l.clock_box.w, l.clock_box.h);
}

static void request_redraw_input(void) {
    ui_layout l;
    compute_layout(&l);
    request_redraw_rect(l.input_box.x, l.input_box.y, l.input_box.w, l.input_box.h);
}

static void request_redraw_log_and_status(void) {
    ui_layout l;
    compute_layout(&l);
    request_redraw_rect(l.log_box.x, l.log_box.y, l.log_box.w, l.log_box.h);
    request_redraw_rect(l.status_box.x, l.status_box.y, l.status_box.w, l.status_box.h);
}

static bool session_logged_in(void) {
    return s_session_user != SESSION_USER_NONE;
}

static const char* session_user_name(void) {
    if (s_session_user == SESSION_USER_ROOT) {
        return "root";
    }
    if (s_session_user == SESSION_USER_GUEST) {
        return "guest";
    }
    return "nobody";
}

static const char* terminal_prompt_text(void) {
    if (s_session_user == SESSION_USER_GUEST) {
        return "guest@pycoreos$ ";
    }
    if (s_session_user == SESSION_USER_ROOT) {
        return "root@pycoreos# ";
    }
    return "login> ";
}

static const char* session_title_label(void) {
    if (s_session_user == SESSION_USER_GUEST) {
        return "GUEST";
    }
    if (s_session_user == SESSION_USER_ROOT) {
        return "ADMIN";
    }
    return "LOCKED";
}

static rect_i login_panel_rect(void) {
    const int w = 500;
    const int h = 310;
    return rect_make((kScreenWidth - w) / 2, (kScreenHeight - h) / 2, w, h);
}

static rect_i login_root_rect(void) {
    const rect_i panel = login_panel_rect();
    return rect_make(panel.x + 24, panel.y + 82, 212, 72);
}

static rect_i login_guest_rect(void) {
    const rect_i panel = login_panel_rect();
    return rect_make(panel.x + panel.w - 236, panel.y + 82, 212, 72);
}

static rect_i login_pin_rect(void) {
    const rect_i panel = login_panel_rect();
    return rect_make(panel.x + 24, panel.y + 182, panel.w - 48, 34);
}

static rect_i login_button_rect(void) {
    const rect_i panel = login_panel_rect();
    return rect_make(panel.x + panel.w - 150, panel.y + panel.h - 48, 124, 28);
}

static void login_reset_state(void) {
    s_login_guest_selected = false;
    s_login_pin_len = 0;
    s_login_pin[0] = '\0';
    s_login_message[0] = '\0';
}

static void login_begin_session(session_user user) {
    s_session_user = user;
    s_input_len = 0;
    s_input_line[0] = '\0';
    s_pending_command[0] = '\0';
    s_has_pending_command = false;
    s_sleeping = false;
    s_start_menu_open = false;
    start_menu_reset_search();
    s_terminal_window.minimized = false;
    s_terminal_window.dragging = false;
    s_terminal_window.resizing = false;
    s_terminal_btn_pressed = 0;
    s_login_message[0] = '\0';
    s_login_pin_len = 0;
    s_login_pin[0] = '\0';

    desktop_clear_log();
    desktop_append_log("PyCoreOS " PYCOREOS_VERSION " (" PYCOREOS_CHANNEL ")");
    if (user == SESSION_USER_ROOT) {
        desktop_append_log("Signed in as root (admin).");
    } else {
        desktop_append_log("Signed in as guest.");
        desktop_append_log("Guest session active.");
    }
    desktop_append_log("PyCoreOS CLI ready. Type 'help'.");
    desktop_append_log("WM enabled: drag/resize terminal, click Start for menu.");
    request_redraw();
}

static void login_attempt(void) {
    if (s_login_guest_selected) {
        login_begin_session(SESSION_USER_GUEST);
        return;
    }

    if (cstr_eq(s_login_pin, "11176")) {
        login_begin_session(SESSION_USER_ROOT);
        return;
    }

    copy_str(s_login_message, sizeof(s_login_message), "Invalid root PIN.");
    s_login_pin_len = 0;
    s_login_pin[0] = '\0';
    request_redraw();
}

static void login_toggle_account(void) {
    s_login_guest_selected = !s_login_guest_selected;
    s_login_message[0] = '\0';
    if (s_login_guest_selected) {
        s_login_pin_len = 0;
        s_login_pin[0] = '\0';
    }
    request_redraw();
}

static bool login_handle_key(char c) {
    if (c == '\t') {
        login_toggle_account();
        return true;
    }

    if (c == '\n' || c == '\r') {
        login_attempt();
        return true;
    }

    if (c == 27) {
        login_reset_state();
        request_redraw();
        return true;
    }

    if (s_login_guest_selected) {
        return true;
    }

    if (c == '\b') {
        if (s_login_pin_len > 0) {
            --s_login_pin_len;
            s_login_pin[s_login_pin_len] = '\0';
            request_redraw();
        }
        return true;
    }

    if (c >= '0' && c <= '9' && s_login_pin_len + 1 < sizeof(s_login_pin)) {
        s_login_pin[s_login_pin_len++] = c;
        s_login_pin[s_login_pin_len] = '\0';
        s_login_message[0] = '\0';
        request_redraw();
    }
    return true;
}

static void login_handle_pointer_click(void) {
    if (rect_contains(login_root_rect(), s_mouse_x, s_mouse_y)) {
        s_login_guest_selected = false;
        s_login_message[0] = '\0';
        request_redraw();
        return;
    }
    if (rect_contains(login_guest_rect(), s_mouse_x, s_mouse_y)) {
        s_login_guest_selected = true;
        s_login_message[0] = '\0';
        s_login_pin_len = 0;
        s_login_pin[0] = '\0';
        request_redraw();
        return;
    }
    if (rect_contains(login_button_rect(), s_mouse_x, s_mouse_y)) {
        login_attempt();
    }
}

static inline void bb_put_pixel(int x, int y, uint32_t color) {
    if (x < 0 || y < 0) {
        return;
    }
    if (x >= kScreenWidth || y >= kScreenHeight) {
        return;
    }
    if (s_clip_enabled && !rect_contains(s_clip_rect, x, y)) {
        return;
    }

    s_draw_target[(size_t)y * kBackbufferMaxW + (size_t)x] = color;
}

static inline void bb_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) {
        return;
    }

    rect_i r = rect_make(x, y, w, h);
    r = rect_intersect(r, rect_make(0, 0, kScreenWidth, kScreenHeight));
    if (s_clip_enabled) {
        r = rect_intersect(r, s_clip_rect);
    }
    if (!rect_valid(r)) {
        return;
    }

    for (int py = r.y; py < (r.y + r.h); ++py) {
        uint32_t* dst = &s_draw_target[(size_t)py * kBackbufferMaxW + (size_t)r.x];
        for (int px = 0; px < r.w; ++px) {
            dst[px] = color;
        }
    }
}

static uint32_t blend_rgb(uint32_t base, uint32_t overlay, uint32_t alpha255) {
    const uint32_t inv = 255U - alpha255;
    const uint32_t br = (base >> 16U) & 0xFFU;
    const uint32_t bg = (base >> 8U) & 0xFFU;
    const uint32_t bb = base & 0xFFU;
    const uint32_t or = (overlay >> 16U) & 0xFFU;
    const uint32_t og = (overlay >> 8U) & 0xFFU;
    const uint32_t ob = overlay & 0xFFU;

    const uint32_t r = (br * inv + or * alpha255) / 255U;
    const uint32_t g = (bg * inv + og * alpha255) / 255U;
    const uint32_t b = (bb * inv + ob * alpha255) / 255U;
    return (r << 16U) | (g << 8U) | b;
}

static void bb_blend_rect(int x, int y, int w, int h, uint32_t color, uint32_t alpha255) {
    if (w <= 0 || h <= 0 || alpha255 == 0U) {
        return;
    }
    if (alpha255 >= 255U) {
        bb_fill_rect(x, y, w, h, color);
        return;
    }

    rect_i r = rect_make(x, y, w, h);
    r = rect_intersect(r, rect_make(0, 0, kScreenWidth, kScreenHeight));
    if (s_clip_enabled) {
        r = rect_intersect(r, s_clip_rect);
    }
    if (!rect_valid(r)) {
        return;
    }

    for (int py = r.y; py < r.y + r.h; ++py) {
        uint32_t* dst = &s_draw_target[(size_t)py * kBackbufferMaxW + (size_t)r.x];
        for (int px = 0; px < r.w; ++px) {
            dst[px] = blend_rgb(dst[px], color, alpha255);
        }
    }
}

static void bb_fill_round_rect(int x, int y, int w, int h, int radius, uint32_t color) {
    if (w <= 0 || h <= 0) {
        return;
    }
    int r = radius;
    if (r < 0) {
        r = 0;
    }
    if (r * 2 > w) {
        r = w / 2;
    }
    if (r * 2 > h) {
        r = h / 2;
    }

    if (r == 0) {
        bb_fill_rect(x, y, w, h, color);
        return;
    }

    bb_fill_rect(x + r, y, w - 2 * r, h, color);
    bb_fill_rect(x, y + r, r, h - 2 * r, color);
    bb_fill_rect(x + w - r, y + r, r, h - 2 * r, color);

    const int rr = r * r;
    for (int dy = 0; dy < r; ++dy) {
        for (int dx = 0; dx < r; ++dx) {
            const int ox = r - 1 - dx;
            const int oy = r - 1 - dy;
            if (ox * ox + oy * oy > rr) {
                continue;
            }
            bb_put_pixel(x + dx, y + dy, color);
            bb_put_pixel(x + w - 1 - dx, y + dy, color);
            bb_put_pixel(x + dx, y + h - 1 - dy, color);
            bb_put_pixel(x + w - 1 - dx, y + h - 1 - dy, color);
        }
    }
}

static void bb_fill_round_rect_alpha(int x, int y, int w, int h, int radius, uint32_t color, uint32_t alpha255) {
    if (alpha255 == 0U) {
        return;
    }
    if (alpha255 >= 255U) {
        bb_fill_round_rect(x, y, w, h, radius, color);
        return;
    }

    int r = radius;
    if (r < 0) {
        r = 0;
    }
    if (r * 2 > w) {
        r = w / 2;
    }
    if (r * 2 > h) {
        r = h / 2;
    }

    if (r == 0) {
        bb_blend_rect(x, y, w, h, color, alpha255);
        return;
    }

    bb_blend_rect(x + r, y, w - 2 * r, h, color, alpha255);
    bb_blend_rect(x, y + r, r, h - 2 * r, color, alpha255);
    bb_blend_rect(x + w - r, y + r, r, h - 2 * r, color, alpha255);

    const int rr = r * r;
    for (int dy = 0; dy < r; ++dy) {
        for (int dx = 0; dx < r; ++dx) {
            const int ox = r - 1 - dx;
            const int oy = r - 1 - dy;
            if (ox * ox + oy * oy > rr) {
                continue;
            }
            bb_blend_rect(x + dx, y + dy, 1, 1, color, alpha255);
            bb_blend_rect(x + w - 1 - dx, y + dy, 1, 1, color, alpha255);
            bb_blend_rect(x + dx, y + h - 1 - dy, 1, 1, color, alpha255);
            bb_blend_rect(x + w - 1 - dx, y + h - 1 - dy, 1, 1, color, alpha255);
        }
    }
}

static void bb_draw_soft_shadow(int x, int y, int w, int h, int radius) {
    bb_fill_round_rect_alpha(x + 3, y + 3, w, h, radius, 0x000000, 72U);
    bb_fill_round_rect_alpha(x + 6, y + 6, w, h, radius, 0x000000, 36U);
}

static void bb_draw_soft_panel(int x, int y, int w, int h, int radius, uint32_t fill, uint32_t border) {
    bb_draw_soft_shadow(x, y, w, h, radius);
    bb_fill_round_rect(x, y, w, h, radius, border);
    bb_fill_round_rect(x + 1, y + 1, w - 2, h - 2, radius - 1, fill);
}

static void bb_copy_rect(uint32_t* dst, const uint32_t* src, rect_i r) {
    if (dst == NULL || src == NULL || !rect_valid(r)) {
        return;
    }

    rect_i clipped = rect_intersect(r, rect_make(0, 0, kScreenWidth, kScreenHeight));
    if (!rect_valid(clipped)) {
        return;
    }

    for (int y = 0; y < clipped.h; ++y) {
        const size_t row = (size_t)(clipped.y + y) * kBackbufferMaxW + (size_t)clipped.x;
        for (int x = 0; x < clipped.w; ++x) {
            dst[row + (size_t)x] = src[row + (size_t)x];
        }
    }
}

static void bb_draw_vgradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom) {
    if (w <= 0 || h <= 0) {
        return;
    }

    const uint32_t den = (h > 1) ? (uint32_t)(h - 1) : 1U;
    for (int row = 0; row < h; ++row) {
        const uint32_t t = (uint32_t)row * 255U / den;
        const uint32_t color = color_lerp(top, bottom, t);
        bb_fill_rect(x, y + row, w, 1, color);
    }
}

static void bb_draw_border(int x, int y, int w, int h, uint32_t color) {
    if (w <= 1 || h <= 1) {
        return;
    }

    bb_fill_rect(x, y, w, 1, color);
    bb_fill_rect(x, y + h - 1, w, 1, color);
    bb_fill_rect(x, y, 1, h, color);
    bb_fill_rect(x + w - 1, y, 1, h, color);
}

static void bb_draw_raised_box(int x, int y, int w, int h, uint32_t fill) {
    if (w <= 2 || h <= 2) {
        return;
    }

    bb_fill_rect(x, y, w, h, fill);
    bb_fill_rect(x, y, w, 1, kPalette.frame_light);
    bb_fill_rect(x, y, 1, h, kPalette.frame_light);
    bb_fill_rect(x + w - 2, y + 1, 1, h - 2, kPalette.frame_dark);
    bb_fill_rect(x + 1, y + h - 2, w - 2, 1, kPalette.frame_dark);
    bb_fill_rect(x + w - 1, y, 1, h, kPalette.frame_darker);
    bb_fill_rect(x, y + h - 1, w, 1, kPalette.frame_darker);
}

static void bb_draw_sunken_box(int x, int y, int w, int h, uint32_t fill) {
    if (w <= 2 || h <= 2) {
        return;
    }

    bb_fill_rect(x, y, w, h, fill);
    bb_fill_rect(x, y, w, 1, kPalette.frame_darker);
    bb_fill_rect(x, y, 1, h, kPalette.frame_darker);
    bb_fill_rect(x + w - 2, y + 1, 1, h - 2, kPalette.frame_light);
    bb_fill_rect(x + 1, y + h - 2, w - 2, 1, kPalette.frame_light);
    bb_fill_rect(x + w - 1, y, 1, h, kPalette.frame_dark);
    bb_fill_rect(x, y + h - 1, w, 1, kPalette.frame_dark);
}

static void draw_char_5x7(int x, int y, char c, uint32_t color, int scale) {
    uint8_t rows[7];
    if (!font5x7_rows_for(c, rows)) {
        return;
    }

    for (int ry = 0; ry < 7; ++ry) {
        const uint8_t row = rows[ry];
        for (int rx = 0; rx < 5; ++rx) {
            if ((row & (1U << (4 - rx))) == 0) {
                continue;
            }
            bb_fill_rect(x + rx * scale, y + ry * scale, scale, scale, color);
        }
    }
}

static int ui_text_scale(int base_scale) {
    if (s_font_profile_16_10_1680x1050 && base_scale == 1) {
        return 2;
    }
    return base_scale;
}

static void draw_text_clipped(int x, int y, const char* text, uint32_t color, int scale, int max_w) {
    if (text == NULL || text[0] == '\0' || scale <= 0 || max_w <= 0) {
        return;
    }

    const int eff_scale = ui_text_scale(scale);
    const int char_w = 6 * eff_scale;
    int max_chars = max_w / char_w;
    if (max_chars <= 0) {
        return;
    }

    for (int i = 0; text[i] != '\0' && i < max_chars; ++i) {
        draw_char_5x7(x + i * char_w, y, text[i], color, eff_scale);
    }
}

static rect_i rect_inset(rect_i r, int inset) {
    rect_i out = r;
    out.x += inset;
    out.y += inset;
    out.w -= inset * 2;
    out.h -= inset * 2;
    if (out.w < 0) {
        out.w = 0;
    }
    if (out.h < 0) {
        out.h = 0;
    }
    return out;
}

static int terminal_cols_for_rect(rect_i r) {
    if (r.w <= 0) {
        return 0;
    }
    return r.w / kTerminalCellW;
}

static int terminal_rows_for_rect(rect_i r) {
    if (r.h <= 0) {
        return 0;
    }
    return r.h / kTerminalCellH;
}

static void terminal_draw_cell_char(rect_i grid, int col, int row, char c, uint32_t color) {
    if (col < 0 || row < 0) {
        return;
    }

    const int cols = terminal_cols_for_rect(grid);
    const int rows = terminal_rows_for_rect(grid);
    if (col >= cols || row >= rows) {
        return;
    }

    const int x = grid.x + col * kTerminalCellW;
    const int y = grid.y + row * kTerminalCellH;
    draw_char_5x7(x + kTerminalGlyphOffsetX, y + kTerminalGlyphOffsetY, c, color, 1);
}

static void terminal_draw_text_line(rect_i grid, int row, int start_col, const char* text, uint32_t color, int max_cols) {
    if (text == NULL || text[0] == '\0' || max_cols <= 0) {
        return;
    }

    int col = start_col;
    for (int i = 0; text[i] != '\0' && (col - start_col) < max_cols; ++i) {
        terminal_draw_cell_char(grid, col, row, text[i], color);
        ++col;
    }
}

static void log_push_line(const char* line) {
    if (line == NULL) {
        return;
    }

    if (s_log_count < kLogLines) {
        copy_str(s_log[s_log_count], sizeof(s_log[s_log_count]), line);
        ++s_log_count;
        s_log_scroll = 0;
        return;
    }

    for (size_t i = 1; i < kLogLines; ++i) {
        copy_str(s_log[i - 1], sizeof(s_log[i - 1]), s_log[i]);
    }
    copy_str(s_log[kLogLines - 1], sizeof(s_log[kLogLines - 1]), line);
    s_log_scroll = 0;
}

static void queue_command(const char* cmd) {
    if (cmd == NULL || cmd[0] == '\0') {
        return;
    }

    if (s_has_pending_command) {
        log_push_line("Shell busy: wait for current command.");
        return;
    }
    copy_str(s_pending_command, sizeof(s_pending_command), cmd);
    s_has_pending_command = true;

    char echo[kLogLineLen];
    copy_str(echo, sizeof(echo), "> ");
    size_t i = cstr_len(echo);
    size_t j = 0;
    while (cmd[j] != '\0' && i + 1 < sizeof(echo)) {
        echo[i++] = cmd[j++];
    }
    echo[i] = '\0';
    log_push_line(echo);
}

static void wm_init_window(void) {
    const int sw = kScreenWidth;
    const int sh = kScreenHeight;
    const int desktop_h = sh - kTaskbarH;

    int w = (int)((uint32_t)sw * 72U / 100U);
    int h = (int)((uint32_t)desktop_h * 72U / 100U);
    if (w < 420) w = 420;
    if (h < 260) h = 260;
    if (w > sw - 20) w = sw - 20;
    if (h > desktop_h - 20) h = desktop_h - 20;
    if (w < kWindowMinW) w = kWindowMinW;
    if (h < kWindowMinH) h = kWindowMinH;

    s_terminal_window.w = w;
    s_terminal_window.h = h;
    s_terminal_window.x = (sw - w) / 2;
    s_terminal_window.y = (desktop_h - h) / 2;
    s_terminal_window.restore_x = s_terminal_window.x;
    s_terminal_window.restore_y = s_terminal_window.y;
    s_terminal_window.restore_w = s_terminal_window.w;
    s_terminal_window.restore_h = s_terminal_window.h;
    s_terminal_window.minimized = false;
    s_terminal_window.maximized = false;
    s_terminal_window.dragging = false;
    s_terminal_window.resizing = false;
    s_terminal_window.resize_edges = 0;
    s_terminal_window.drag_dx = 0;
    s_terminal_window.drag_dy = 0;
    s_terminal_window.resize_anchor_mouse_x = 0;
    s_terminal_window.resize_anchor_mouse_y = 0;
    s_terminal_window.resize_anchor_x = 0;
    s_terminal_window.resize_anchor_y = 0;
    s_terminal_window.resize_anchor_w = 0;
    s_terminal_window.resize_anchor_h = 0;
}

static void wm_toggle_maximize(void) {
    const int sw = kScreenWidth;
    const int desktop_h = kScreenHeight - kTaskbarH;
    const int max_w = sw - 4;
    const int max_h = desktop_h - 4;

    if (!s_terminal_window.maximized) {
        s_terminal_window.restore_x = s_terminal_window.x;
        s_terminal_window.restore_y = s_terminal_window.y;
        s_terminal_window.restore_w = s_terminal_window.w;
        s_terminal_window.restore_h = s_terminal_window.h;

        s_terminal_window.maximized = true;
        s_terminal_window.x = 2;
        s_terminal_window.y = 2;
        s_terminal_window.w = max_w;
        s_terminal_window.h = max_h;
    } else {
        s_terminal_window.maximized = false;
        if (s_terminal_window.restore_w > 0 && s_terminal_window.restore_h > 0) {
            s_terminal_window.x = s_terminal_window.restore_x;
            s_terminal_window.y = s_terminal_window.restore_y;
            s_terminal_window.w = s_terminal_window.restore_w;
            s_terminal_window.h = s_terminal_window.restore_h;
        } else {
            wm_init_window();
        }
    }

    s_terminal_window.dragging = false;
    s_terminal_window.resizing = false;
}

static void compute_layout(ui_layout* out) {
    const int sw = kScreenWidth;
    const int sh = kScreenHeight;
    const int desktop_h = sh - kTaskbarH;

    if (s_terminal_window.w <= 0 || s_terminal_window.h <= 0) {
        wm_init_window();
    }

    if (s_terminal_window.maximized) {
        s_terminal_window.x = 2;
        s_terminal_window.y = 2;
        s_terminal_window.w = sw - 4;
        s_terminal_window.h = desktop_h - 4;
    } else {
        if (s_terminal_window.w > sw - 20) s_terminal_window.w = sw - 20;
        if (s_terminal_window.h > desktop_h - 20) s_terminal_window.h = desktop_h - 20;
        if (s_terminal_window.w < kWindowMinW) s_terminal_window.w = kWindowMinW;
        if (s_terminal_window.h < kWindowMinH) s_terminal_window.h = kWindowMinH;

        s_terminal_window.x = clamp_i32(s_terminal_window.x, 2, sw - s_terminal_window.w - 2);
        s_terminal_window.y = clamp_i32(s_terminal_window.y, 2, desktop_h - s_terminal_window.h - 2);
    }

    if (s_terminal_window.w < kWindowMinW) s_terminal_window.w = kWindowMinW;
    if (s_terminal_window.h < kWindowMinH) s_terminal_window.h = kWindowMinH;

    out->screen_w = sw;
    out->screen_h = sh;
    out->taskbar = rect_make(0, sh - kTaskbarH, sw, kTaskbarH);
    out->start_button = rect_make(6, out->taskbar.y + 5, 74, 24);
    out->task_terminal_button = rect_make(88, out->taskbar.y + 5, 86, 24);
    const int quick_y = out->taskbar.y + 5;
    out->quick_help_button = rect_make(180, quick_y, kQuickLaunchW, 24);
    out->quick_files_button = rect_make(out->quick_help_button.x + kQuickLaunchW + kQuickLaunchGap, quick_y, kQuickLaunchW, 24);
    out->quick_doom_button = rect_make(out->quick_files_button.x + kQuickLaunchW + kQuickLaunchGap, quick_y, kQuickLaunchW, 24);
    out->clock_box = rect_make(sw - 94, out->taskbar.y + 5, 86, 24);

    const int menu_h = kStartMenuHeaderH + (kStartMenuItems * kStartMenuItemH) + 8;
    int menu_y = out->start_button.y - menu_h - 2;
    if (menu_y < 2) {
        menu_y = 2;
    }
    out->start_menu = rect_make(6, menu_y, 284, menu_h);

    out->window = rect_make(s_terminal_window.x, s_terminal_window.y, s_terminal_window.w, s_terminal_window.h);
    out->titlebar = rect_make(out->window.x + 3, out->window.y + 3, out->window.w - 6, kTitlebarH);
    {
        const int pad = kTitleBarButtonPadding;
        const int sz = kTitleBarButtonSize;
        const int right = out->window.x + out->window.w;
        out->btn_close = rect_make(right - pad - sz, out->window.y + pad, sz, sz);
        out->btn_max  = rect_make(right - pad - sz - pad - sz, out->window.y + pad, sz, sz);
        out->btn_min  = rect_make(right - pad - sz - pad - sz - pad - sz, out->window.y + pad, sz, sz);
    }

    out->log_box = rect_make(out->window.x + 10, out->window.y + kTitlebarH + 12, out->window.w - 20,
                             out->window.h - kTitlebarH - kInputH - kStatusH - 24);
    out->input_box = rect_make(out->window.x + 10, out->window.y + out->window.h - kStatusH - kInputH - 8,
                               out->window.w - 20, kInputH);
    out->status_box = rect_make(out->window.x + 10, out->window.y + out->window.h - kStatusH - 4,
                                out->window.w - 20, kStatusH);
}

static rect_i start_menu_search_rect(const ui_layout* l) {
    return rect_make(l->start_menu.x + 8, l->start_menu.y + 24, 114, 14);
}

static rect_i start_menu_quick_rect(const ui_layout* l, int slot) {
    return rect_make(l->start_menu.x + 128 + slot * 46, l->start_menu.y + 24, 42, 14);
}

static rect_i start_menu_power_rect(const ui_layout* l, int slot) {
    return rect_make(l->start_menu.x + l->start_menu.w - 113 + slot * 27, l->start_menu.y + 6, 24, 14);
}

static bool start_menu_item_matches(int item_idx) {
    if (item_idx < 0 || item_idx >= kStartMenuItems) {
        return false;
    }
    if (s_start_search_len == 0) {
        return true;
    }
    return cstr_contains_icase(kStartMenuLabels[item_idx], s_start_search);
}

static rect_i start_menu_item_rect(const ui_layout* l, int visible_row) {
    return rect_make(l->start_menu.x + 6,
                     l->start_menu.y + kStartMenuHeaderH + 4 + visible_row * kStartMenuItemH,
                     l->start_menu.w - 12,
                     kStartMenuItemH - 1);
}

static int start_menu_first_visible_item(void) {
    for (int i = 0; i < kStartMenuItems; ++i) {
        if (start_menu_item_matches(i)) {
            return i;
        }
    }
    return -1;
}

static int start_menu_item_index_at(const ui_layout* l, int x, int y) {
    int row = 0;
    for (int i = 0; i < kStartMenuItems; ++i) {
        if (!start_menu_item_matches(i)) {
            continue;
        }
        if (rect_contains(start_menu_item_rect(l, row), x, y)) {
            return i;
        }
        ++row;
    }
    return -1;
}

static void start_menu_reset_search(void) {
    s_start_search[0] = '\0';
    s_start_search_len = 0;
    s_start_search_focused = false;
}

static void app_windows_init(void) {
    for (int i = 0; i < kAppWindowCount; ++i) {
        s_app_windows[i].x = 0;
        s_app_windows[i].y = 0;
        s_app_windows[i].w = kAppWindowW;
        s_app_windows[i].h = kAppWindowH;
        s_app_windows[i].open = false;
        s_app_windows[i].minimized = false;
        s_app_windows[i].maximized = false;
        s_app_windows[i].prev_x = 0;
        s_app_windows[i].prev_y = 0;
        s_app_windows[i].prev_w = kAppWindowW;
        s_app_windows[i].prev_h = kAppWindowH;
        s_app_z_order[i] = i;
    }
    s_drag_app_idx = -1;
    s_drag_app_dx = 0;
    s_drag_app_dy = 0;
}

static int app_z_pos(int app_idx) {
    for (int i = 0; i < kAppWindowCount; ++i) {
        if (s_app_z_order[i] == app_idx) {
            return i;
        }
    }
    return -1;
}

static void app_bring_to_front(int app_idx) {
    const int pos = app_z_pos(app_idx);
    if (pos < 0 || pos == kAppWindowCount - 1) {
        return;
    }

    const int entry = s_app_z_order[pos];
    for (int i = pos; i < kAppWindowCount - 1; ++i) {
        s_app_z_order[i] = s_app_z_order[i + 1];
    }
    s_app_z_order[kAppWindowCount - 1] = entry;
}

static rect_i app_window_rect(int app_idx) {
    const app_window* w = &s_app_windows[app_idx];
    return rect_make(w->x, w->y, w->w, w->h);
}

static rect_i app_window_title_rect(int app_idx) {
    const app_window* w = &s_app_windows[app_idx];
    return rect_make(w->x + 3, w->y + 3, w->w - 6, kAppWindowTitleH);
}

static rect_i app_window_close_rect(int app_idx) {
    const rect_i title = app_window_title_rect(app_idx);
    const int pad = kTitleBarButtonPadding;
    const int sz = kTitleBarButtonSize;
    return rect_make(title.x + title.w - pad - sz, title.y + pad, sz, sz);
}

static rect_i app_window_max_rect(int app_idx) {
    const rect_i title = app_window_title_rect(app_idx);
    const int pad = kTitleBarButtonPadding;
    const int sz = kTitleBarButtonSize;
    return rect_make(title.x + title.w - pad - sz - pad - sz, title.y + pad, sz, sz);
}

static rect_i app_window_min_rect(int app_idx) {
    const rect_i title = app_window_title_rect(app_idx);
    const int pad = kTitleBarButtonPadding;
    const int sz = kTitleBarButtonSize;
    return rect_make(title.x + title.w - pad - sz - pad - sz - pad - sz, title.y + pad, sz, sz);
}

static rect_i app_window_resize_rect(int app_idx) {
    const app_window* w = &s_app_windows[app_idx];
    return rect_make(w->x + w->w - 14, w->y + w->h - 14, 12, 12);
}

static rect_i app_window_content_rect(int app_idx) {
    const app_window* w = &s_app_windows[app_idx];
    return rect_make(w->x + 8, w->y + kAppWindowTitleH + 10, w->w - 16, w->h - kAppWindowTitleH - 14);
}

static void clamp_app_window_to_desktop(app_window* w, const ui_layout* l) {
    if (w->w > l->screen_w - 8) {
        w->w = l->screen_w - 8;
    }
    if (w->h > l->taskbar.y - 8) {
        w->h = l->taskbar.y - 8;
    }
    if (w->w < 180) {
        w->w = 180;
    }
    if (w->h < 120) {
        w->h = 120;
    }

    int max_x = l->screen_w - w->w - 2;
    int max_y = l->taskbar.y - w->h - 2;
    if (max_x < 2) {
        max_x = 2;
    }
    if (max_y < 2) {
        max_y = 2;
    }

    w->x = clamp_i32(w->x, 2, max_x);
    w->y = clamp_i32(w->y, 2, max_y);
}

static int topmost_app_window_at_point(int x, int y) {
    for (int z = kAppWindowCount - 1; z >= 0; --z) {
        const int app_idx = s_app_z_order[z];
        if (!s_app_windows[app_idx].open || s_app_windows[app_idx].minimized) {
            continue;
        }
        if (rect_contains(app_window_rect(app_idx), x, y)) {
            return app_idx;
        }
    }
    return -1;
}

static void open_app_window(app_id app) {
    const int app_idx = (int)app;
    if (app_idx < 0 || app_idx >= kAppWindowCount) {
        return;
    }

    ui_layout l;
    compute_layout(&l);

    app_window* w = &s_app_windows[app_idx];
    if (!w->open) {
        const int col = app_idx % 3;
        const int row = app_idx / 3;
        w->w = kAppWindowW;
        w->h = kAppWindowH;
        w->x = l.screen_w - w->w - 24 - col * 18;
        w->y = 26 + row * 22;
    }
    w->open = true;
    w->minimized = false;
    w->maximized = false;
    clamp_app_window_to_desktop(w, &l);
    app_bring_to_front(app_idx);
    s_active_is_terminal = false;
    s_active_app_idx = app_idx;
    request_redraw();
}

static void close_app_window(app_id app) {
    const int app_idx = (int)app;
    if (app_idx < 0 || app_idx >= kAppWindowCount) {
        return;
    }
    if (!s_app_windows[app_idx].open) {
        return;
    }

    const rect_i win_rect = app_window_rect(app_idx);
    request_redraw_rect(win_rect.x, win_rect.y, win_rect.w, win_rect.h);

    s_app_windows[app_idx].open = false;
    s_app_windows[app_idx].minimized = false;
    s_app_windows[app_idx].maximized = false;
    if (s_drag_app_idx == app_idx) {
        s_drag_app_idx = -1;
    }
    if (s_active_app_idx == app_idx) {
        s_active_app_idx = -1;
        s_active_is_terminal = true;
    }
    if (s_app_btn_pressed_idx == app_idx) {
        s_app_btn_pressed_idx = -1;
    }
}

static void set_app_minimized(app_id app, bool minimized) {
    const int app_idx = (int)app;
    if (app_idx < 0 || app_idx >= kAppWindowCount) {
        return;
    }
    if (!s_app_windows[app_idx].open) {
        return;
    }

    s_app_windows[app_idx].minimized = minimized;
    if (!minimized) {
        app_bring_to_front(app_idx);
        s_active_is_terminal = false;
        s_active_app_idx = app_idx;
    }
    if (s_drag_app_idx == app_idx) {
        s_drag_app_idx = -1;
    }
    request_redraw();
}

static void wm_dispatch_app_message(int app_idx, wm_message msg) {
    if (app_idx < 0 || app_idx >= kAppWindowCount || !s_app_windows[app_idx].open) {
        return;
    }
    app_window* w = &s_app_windows[app_idx];
    ui_layout l;
    compute_layout(&l);

    switch (msg) {
    case WM_CLOSE:
        close_app_window((app_id)app_idx);
        return;
    case WM_MINIMIZE:
        set_app_minimized((app_id)app_idx, true);
        return;
    case WM_MAXIMIZE:
        if (!w->maximized) {
            w->prev_x = w->x;
            w->prev_y = w->y;
            w->prev_w = w->w;
            w->prev_h = w->h;
            w->x = 0;
            w->y = 0;
            w->w = l.screen_w;
            w->h = l.taskbar.y;
            w->maximized = true;
        } else {
            w->x = w->prev_x;
            w->y = w->prev_y;
            w->w = w->prev_w > 0 ? w->prev_w : kAppWindowW;
            w->h = w->prev_h > 0 ? w->prev_h : kAppWindowH;
            w->maximized = false;
            clamp_app_window_to_desktop(w, &l);
        }
        if (s_drag_app_idx == app_idx) {
            s_drag_app_idx = -1;
        }
        request_redraw();
        return;
    case WM_RESTORE:
        if (w->maximized) {
            w->x = w->prev_x;
            w->y = w->prev_y;
            w->w = w->prev_w > 0 ? w->prev_w : kAppWindowW;
            w->h = w->prev_h > 0 ? w->prev_h : kAppWindowH;
            w->maximized = false;
            clamp_app_window_to_desktop(w, &l);
        }
        if (s_drag_app_idx == app_idx) {
            s_drag_app_idx = -1;
        }
        request_redraw();
        return;
    default:
        break;
    }
}

static void editor_set_buffer(const char* filename, const char* text) {
    copy_str(s_editor_filename, sizeof(s_editor_filename), filename != NULL ? filename : "");
    size_t n = 0;
    if (text != NULL) {
        while (text[n] != '\0' && n + 1 < sizeof(s_editor_text)) {
            s_editor_text[n] = text[n];
            ++n;
        }
    }
    s_editor_text[n] = '\0';
    s_editor_len = n;
    s_editor_cursor = n;
    s_editor_dirty = false;
}

static bool is_text_file_name(const char* name) {
    if (name == NULL) {
        return false;
    }
    return cstr_ends_with(name, ".txt") ||
           cstr_ends_with(name, ".cfg") ||
           cstr_ends_with(name, ".md") ||
           cstr_ends_with(name, ".log");
}

static void editor_open_file(const char* filename) {
    char text[kEditorMax];
    if (!fs_read(filename, text, sizeof(text))) {
        copy_str(text, sizeof(text), "(unable to read file)");
    }
    editor_set_buffer(filename, text);
    open_app_window(APP_EDITOR);
    app_bring_to_front(APP_EDITOR);
    s_editor_focused = true;
}

static void editor_save(void) {
    if (s_editor_filename[0] == '\0') {
        return;
    }
    (void)fs_write_bytes(s_editor_filename, s_editor_text, s_editor_len);
    s_editor_dirty = false;
}

static bool app_id_from_name(const char* name, app_id* out_app) {
    if (name == NULL || out_app == NULL) {
        return false;
    }

    if (cstr_eq(name, "help")) {
        *out_app = APP_HELP;
        return true;
    }
    if (cstr_eq(name, "files")) {
        *out_app = APP_FILES;
        return true;
    }
    if (cstr_eq(name, "system")) {
        *out_app = APP_SYSTEM;
        return true;
    }
    if (cstr_eq(name, "mouse")) {
        *out_app = APP_MOUSE;
        return true;
    }
    if (cstr_eq(name, "settings")) {
        *out_app = APP_SETTINGS;
        return true;
    }
    if (cstr_eq(name, "performance") || cstr_eq(name, "perf")) {
        *out_app = APP_PERFORMANCE;
        return true;
    }
    if (cstr_eq(name, "notes")) {
        *out_app = APP_NOTES;
        return true;
    }
    if (cstr_eq(name, "about")) {
        *out_app = APP_ABOUT;
        return true;
    }
    if (cstr_eq(name, "credits")) {
        *out_app = APP_CREDITS;
        return true;
    }
    if (cstr_eq(name, "tips")) {
        *out_app = APP_TIPS;
        return true;
    }
    if (cstr_eq(name, "doom")) {
        *out_app = APP_DOOM;
        return true;
    }
    if (cstr_eq(name, "editor")) {
        *out_app = APP_EDITOR;
        return true;
    }
    if (cstr_eq(name, "calculator") || cstr_eq(name, "calc")) {
        *out_app = APP_CALCULATOR;
        return true;
    }
    if (cstr_eq(name, "clock")) {
        *out_app = APP_CLOCK;
        return true;
    }
    if (cstr_eq(name, "calendar") || cstr_eq(name, "cal")) {
        *out_app = APP_CALENDAR;
        return true;
    }
    if (cstr_eq(name, "tasks")) {
        *out_app = APP_TASKS;
        return true;
    }
    if (cstr_eq(name, "clipboard") || cstr_eq(name, "clip")) {
        *out_app = APP_CLIPBOARD;
        return true;
    }
    if (cstr_eq(name, "network") || cstr_eq(name, "net")) {
        *out_app = APP_NETWORK;
        return true;
    }
    if (cstr_eq(name, "storage") || cstr_eq(name, "disk")) {
        *out_app = APP_STORAGE;
        return true;
    }
    if (cstr_eq(name, "diagnostics") || cstr_eq(name, "diag")) {
        *out_app = APP_DIAGNOSTICS;
        return true;
    }
    if (cstr_eq(name, "monitor")) {
        *out_app = APP_MONITOR;
        return true;
    }
    if (cstr_eq(name, "guide")) {
        *out_app = APP_TERMINAL_GUIDE;
        return true;
    }
    if (cstr_eq(name, "wallpaper")) {
        *out_app = APP_WALLPAPER;
        return true;
    }
    if (cstr_eq(name, "shortcuts")) {
        *out_app = APP_SHORTCUTS;
        return true;
    }
    if (cstr_eq(name, "troubleshoot")) {
        *out_app = APP_TROUBLESHOOT;
        return true;
    }
    if (cstr_eq(name, "release") || cstr_eq(name, "releasenotes")) {
        *out_app = APP_RELEASE_NOTES;
        return true;
    }
    if (cstr_eq(name, "roadmap")) {
        *out_app = APP_ROADMAP;
        return true;
    }
    if (cstr_eq(name, "journal")) {
        *out_app = APP_JOURNAL;
        return true;
    }
    if (cstr_eq(name, "todo")) {
        *out_app = APP_TODO;
        return true;
    }
    if (cstr_eq(name, "packages") || cstr_eq(name, "pkg")) {
        *out_app = APP_PACKAGES;
        return true;
    }
    if (cstr_eq(name, "snapshots") || cstr_eq(name, "snapshot")) {
        *out_app = APP_SNAPSHOTS;
        return true;
    }
    if (cstr_eq(name, "launcher")) {
        *out_app = APP_LAUNCHER;
        return true;
    }

    return false;
}

static rect_i desktop_icon_default_cell_rect(const ui_layout* l, int index) {
    const int col = index % kDesktopIconCols;
    const int row = index / kDesktopIconCols;
    const int rows = (kDesktopIconCount + kDesktopIconCols - 1) / kDesktopIconCols;

    const int grid_w = kDesktopIconCols * kDesktopIconCellW;
    const int grid_h = rows * kDesktopIconCellH;

    int origin_x = (l->screen_w - grid_w) / 2;
    if (origin_x < 8) {
        origin_x = 8;
    }

    int usable_top = kDesktopIconTopPad;
    int usable_bottom = l->taskbar.y - kDesktopIconBottomPad;
    if (usable_bottom < usable_top) {
        usable_bottom = usable_top;
    }
    const int usable_h = usable_bottom - usable_top;

    int origin_y = usable_top;
    if (grid_h < usable_h) {
        origin_y += (usable_h - grid_h) / 2;
    }

    const int x = origin_x + col * kDesktopIconCellW;
    const int y = origin_y + row * kDesktopIconCellH;
    return rect_make(x, y, kDesktopIconCellW, kDesktopIconCellH);
}

static void ensure_desktop_icon_positions(const ui_layout* l) {
    if (s_icons_initialized) {
        return;
    }
    for (int i = 0; i < kDesktopIconCount; ++i) {
        s_icon_cells[i] = desktop_icon_default_cell_rect(l, i);
    }
    s_icons_initialized = true;
}

static rect_i desktop_icon_cell_rect(const ui_layout* l, int index) {
    if (index < 0 || index >= kDesktopIconCount) {
        return rect_make(0, 0, 0, 0);
    }
    ensure_desktop_icon_positions(l);
    return s_icon_cells[index];
}

static rect_i desktop_icon_logo_rect(const ui_layout* l, int index) {
    const rect_i cell = desktop_icon_cell_rect(l, index);
    const int x = cell.x + (cell.w - kDesktopIconSize) / 2;
    const int y = cell.y + 4;
    return rect_make(x, y, kDesktopIconSize, kDesktopIconSize);
}

static rect_i desktop_icon_hit_rect(const ui_layout* l, int index) {
    const rect_i cell = desktop_icon_cell_rect(l, index);
    int x = cell.x + (cell.w - kDesktopIconLabelW) / 2;
    if (x < 0) x = 0;
    int w = kDesktopIconLabelW;
    if (x + w > l->screen_w) w = l->screen_w - x;
    if (w < 1) w = 1;
    return rect_make(x, cell.y + 1, w, cell.h - 2);
}

static int desktop_icon_closest_default_slot(const ui_layout* l, int icon_idx) {
    if (icon_idx < 0 || icon_idx >= kDesktopIconCount) {
        return -1;
    }

    const rect_i cell = s_icon_cells[icon_idx];
    const int cx = cell.x + cell.w / 2;
    const int cy = cell.y + cell.h / 2;

    int best = 0;
    uint32_t best_dist = 0xFFFFFFFFU;
    for (int i = 0; i < kDesktopIconCount; ++i) {
        const rect_i target = desktop_icon_default_cell_rect(l, i);
        const int tx = target.x + target.w / 2;
        const int ty = target.y + target.h / 2;
        const int dx = cx - tx;
        const int dy = cy - ty;
        const uint32_t dist = (uint32_t)(dx * dx + dy * dy);
        if (dist < best_dist) {
            best_dist = dist;
            best = i;
        }
    }

    return best;
}

static void snap_icon_to_grid(const ui_layout* l, int icon_idx) {
    if (icon_idx < 0 || icon_idx >= kDesktopIconCount) {
        return;
    }

    const int slot = desktop_icon_closest_default_slot(l, icon_idx);
    if (slot < 0) {
        return;
    }

    const rect_i previous = s_icon_cells[icon_idx];
    const rect_i snapped = desktop_icon_default_cell_rect(l, slot);
    int occupant = -1;
    for (int i = 0; i < kDesktopIconCount; ++i) {
        if (i == icon_idx) {
            continue;
        }
        if (s_icon_cells[i].x == snapped.x && s_icon_cells[i].y == snapped.y) {
            occupant = i;
            break;
        }
    }

    s_icon_cells[icon_idx] = snapped;
    if (occupant >= 0) {
        s_icon_cells[occupant] = previous;
    }
}

static uint32_t desktop_icon_accent_color(int index) {
    const uint32_t r = 70U + (uint32_t)((index * 37) % 156);
    const uint32_t g = 64U + (uint32_t)((index * 53) % 140);
    const uint32_t b = 74U + (uint32_t)((index * 29) % 126);
    return (r << 16U) | (g << 8U) | b;
}

static uint32_t hover_anim_t(int phase_offset) {
    const uint32_t period = 48U;
    const uint32_t half = period / 2U;
    const uint32_t phase = (s_ticks + (uint32_t)phase_offset) % period;
    if (phase < half) {
        return (phase * 255U) / half;
    }
    return ((period - phase) * 255U) / half;
}

static void draw_icon_symbol(const rect_i* logo, int app_idx, uint32_t fg, uint32_t alt) {
    const rect_i inner = rect_make(logo->x + 4, logo->y + 4, logo->w - 8, logo->h - 8);
    if (inner.w < 12 || inner.h < 12) {
        return;
    }

    const int cx = inner.x + inner.w / 2;
    const int cy = inner.y + inner.h / 2;
    const uint32_t shade = 0x0C1A2C;

    switch (app_idx % 12) {
        case 0: {
            bb_fill_round_rect(inner.x + 1, inner.y + 6, inner.w - 2, inner.h - 7, 2, fg);
            bb_fill_round_rect(inner.x + 4, inner.y + 3, inner.w / 2, 5, 2, alt);
            break;
        }
        case 1: {
            bb_fill_rect(inner.x + 2, inner.y + inner.h - 6, 3, 5, fg);
            bb_fill_rect(inner.x + 7, inner.y + inner.h - 9, 3, 8, alt);
            bb_fill_rect(inner.x + 12, inner.y + inner.h - 12, 3, 11, fg);
            bb_fill_rect(inner.x + 2, inner.y + inner.h - 1, inner.w - 4, 1, alt);
            break;
        }
        case 2: {
            bb_fill_rect(cx - 1, inner.y + 2, 2, inner.h - 4, fg);
            bb_fill_rect(inner.x + 2, cy - 1, inner.w - 4, 2, fg);
            bb_fill_round_rect(cx - 3, cy - 3, 6, 6, 2, alt);
            bb_fill_rect(cx - 1, cy - 1, 2, 2, shade);
            break;
        }
        case 3: {
            bb_fill_rect(inner.x + 2, inner.y + inner.h - 4, inner.w - 4, 2, fg);
            bb_fill_rect(inner.x + 4, inner.y + inner.h - 7, inner.w - 8, 2, alt);
            bb_fill_rect(inner.x + 6, inner.y + inner.h - 10, inner.w - 12, 2, fg);
            bb_fill_rect(cx - 1, inner.y + 3, 2, inner.h - 12, alt);
            break;
        }
        case 4: {
            bb_fill_round_rect(inner.x + 3, inner.y + 2, inner.w - 6, inner.h - 4, 2, fg);
            bb_fill_rect(inner.x + 6, inner.y + 6, inner.w - 12, 1, alt);
            bb_fill_rect(inner.x + 6, inner.y + 9, inner.w - 9, 1, alt);
            bb_fill_rect(inner.x + 6, inner.y + 12, inner.w - 11, 1, alt);
            break;
        }
        case 5: {
            bb_fill_round_rect(inner.x + 3, inner.y + 3, inner.w - 6, inner.h - 6, 5, fg);
            bb_fill_round_rect(inner.x + 5, inner.y + 5, inner.w - 10, inner.h - 10, 4, alt);
            bb_fill_rect(cx, cy - 4, 1, 4, shade);
            bb_fill_rect(cx, cy, 4, 1, shade);
            break;
        }
        case 6: {
            bb_fill_round_rect(inner.x + 2, inner.y + 3, inner.w - 4, inner.h - 6, 2, fg);
            bb_fill_rect(inner.x + 4, inner.y + 7, 3, 1, alt);
            bb_fill_rect(inner.x + 6, inner.y + 8, 1, 1, alt);
            bb_fill_rect(inner.x + 4, inner.y + 9, 3, 1, alt);
            bb_fill_rect(inner.x + 9, inner.y + 11, inner.w - 13, 1, alt);
            break;
        }
        case 7: {
            bb_fill_round_rect(inner.x + 2, inner.y + 3, inner.w - 4, inner.h - 6, 2, fg);
            bb_fill_round_rect(inner.x + 4, inner.y + 5, inner.w - 8, inner.h - 10, 1, alt);
            bb_fill_rect(inner.x + 6, inner.y + inner.h - 8, 4, 2, shade);
            bb_fill_rect(inner.x + 10, inner.y + inner.h - 10, 4, 4, shade);
            bb_fill_rect(inner.x + 13, inner.y + 6, 2, 2, fg);
            break;
        }
        case 8: {
            bb_fill_round_rect(inner.x + 2, inner.y + 2, inner.w - 4, inner.h - 4, 2, fg);
            bb_fill_rect(inner.x + 5, inner.y + 6, 2, 2, alt);
            bb_fill_rect(inner.x + 8, inner.y + 6, inner.w - 12, 1, alt);
            bb_fill_rect(inner.x + 5, inner.y + 10, 2, 2, alt);
            bb_fill_rect(inner.x + 8, inner.y + 10, inner.w - 12, 1, alt);
            bb_fill_rect(inner.x + 5, inner.y + 14, 2, 2, alt);
            bb_fill_rect(inner.x + 8, inner.y + 14, inner.w - 12, 1, alt);
            break;
        }
        case 9: {
            bb_fill_round_rect(inner.x + 4, inner.y + 8, inner.w - 8, inner.h - 9, 2, fg);
            bb_fill_rect(inner.x + 6, inner.y + 4, inner.w - 12, 5, alt);
            bb_fill_rect(cx - 1, inner.y + 11, 2, 3, shade);
            break;
        }
        case 10: {
            bb_fill_rect(cx - 1, inner.y + 3, 2, inner.h - 8, fg);
            bb_fill_rect(cx - 3, inner.y + 8, 2, 4, alt);
            bb_fill_rect(cx + 1, inner.y + 8, 2, 4, alt);
            bb_fill_rect(cx - 1, inner.y + inner.h - 5, 2, 2, alt);
            bb_fill_rect(cx - 2, inner.y + inner.h - 3, 4, 2, shade);
            break;
        }
        default: {
            bb_fill_rect(cx - 1, inner.y + 3, 2, inner.h - 6, fg);
            bb_fill_rect(inner.x + 3, cy - 1, inner.w - 6, 2, fg);
            bb_fill_rect(inner.x + 5, inner.y + 5, 1, 1, alt);
            bb_fill_rect(inner.x + inner.w - 6, inner.y + 5, 1, 1, alt);
            bb_fill_rect(inner.x + 5, inner.y + inner.h - 6, 1, 1, alt);
            bb_fill_rect(inner.x + inner.w - 6, inner.y + inner.h - 6, 1, 1, alt);
            break;
        }
    }

    const uint32_t seed = (uint32_t)(app_idx + 1) * 2654435761U;
    for (int i = 0; i < 4; ++i) {
        if ((seed & (1U << i)) == 0U) {
            continue;
        }
        bb_fill_rect(inner.x + 1 + i * 3, inner.y + inner.h - 3, 2, 2, alt);
    }
}

static void draw_app_icon_badge(const rect_i* logo, int app_idx, bool hover) {
    const uint32_t accent = desktop_icon_accent_color(app_idx);
    const uint32_t pulse = hover ? hover_anim_t(app_idx * 7) : 96U;
    const uint32_t bg = color_lerp(0x1B2D4B, accent, 96U + pulse / 3U);
    const uint32_t border = color_lerp(0x0B1628, accent, 180U);
    const uint32_t fg = color_lerp(0xE8F4FF, 0xFFFFFF, hover ? pulse / 2U : 128U);
    const uint32_t alt = color_lerp(0x7CA4CF, accent, 182U);

    bb_fill_round_rect(logo->x, logo->y, logo->w, logo->h, 6, border);
    bb_fill_round_rect(logo->x + 1, logo->y + 1, logo->w - 2, logo->h - 2, 5, bg);
    draw_icon_symbol(logo, app_idx, fg, alt);
    if (hover) {
        bb_fill_round_rect_alpha(logo->x, logo->y, logo->w, logo->h, 6, 0xFFFFFF, 34U + pulse / 6U);
    }
}

static uint32_t start_menu_accent_color(int item_idx) {
    if (item_idx >= 0 && item_idx < kStartMenuItems) {
        const int action = kStartMenuActions[item_idx];
        if (action >= 0 && action < kDesktopIconCount) {
            return desktop_icon_accent_color(action);
        }
    }
    return 0x3A6EA5;
}

static void draw_start_menu_icon(const rect_i* r, int item_idx) {
    const uint32_t accent = start_menu_accent_color(item_idx);
    const uint32_t border = color_lerp(0x0B1628, accent, 180U);
    const uint32_t bg = color_lerp(0xE8F4FF, accent, 140U);
    const uint32_t fg = color_lerp(0x1B2D4B, accent, 210U);

    bb_fill_round_rect(r->x, r->y, r->w, r->h, 3, border);
    bb_fill_round_rect(r->x + 1, r->y + 1, r->w - 2, r->h - 2, 3, bg);

    bb_fill_rect(r->x + r->w / 2 - 1, r->y + 2, 2, r->h - 4, fg);
    bb_fill_rect(r->x + 3, r->y + r->h - 4, r->w - 6, 2, border);
}

static void draw_desktop_icons(const ui_layout* l) {
    ensure_desktop_icon_positions(l);

    for (int i = 0; i < kDesktopIconCount; ++i) {
        const rect_i hit = desktop_icon_hit_rect(l, i);
        const rect_i logo = desktop_icon_logo_rect(l, i);
        const app_id app = kDesktopIconApps[i];
        const bool hover = rect_contains(hit, s_mouse_x, s_mouse_y);

        if (hover) {
            bb_fill_round_rect_alpha(hit.x, hit.y, hit.w, hit.h, 6, 0xBFD9F5, 72U);
        }
        draw_app_icon_badge(&logo, (int)app, hover);

        const char* label = kDesktopIconLabels[i];
        const int scale = 1;
        const int eff_scale = ui_text_scale(scale);
        const int char_w = 6 * eff_scale;
        const int text_w = (int)cstr_len(label) * char_w;

        const int center_x = logo.x + logo.w / 2;
        const int text_x = center_x - text_w / 2;
        const int text_y = logo.y + logo.h + 6;

        draw_text_clipped(text_x, text_y, label, hover ? 0xFFFFFF : 0xE9F3FF, scale, text_w + 12);
    }
}

static uint32_t app_window_accent_top(int app_idx) {
    if (app_idx == APP_HELP) return 0x2A7BBE;
    if (app_idx == APP_FILES) return 0x3E8A5F;
    if (app_idx == APP_SYSTEM) return 0x3C6CB5;
    if (app_idx == APP_MOUSE) return 0x7B6BB2;
    if (app_idx == APP_SETTINGS) return 0x8B6E2E;
    if (app_idx == APP_PERFORMANCE) return 0x2E7A5E;
    if (app_idx == APP_NOTES) return 0x5B78A8;
    if (app_idx == APP_ABOUT) return 0x3D5E8A;
    if (app_idx == APP_TIPS) return 0x4F7A56;
    if (app_idx == APP_DOOM) return 0x7A2E2E;
    if (app_idx == APP_CREDITS) return 0xA17624;
    if (app_idx == APP_CALCULATOR) return 0x2E6E8D;
    if (app_idx == APP_CLOCK) return 0x4E6FA8;
    if (app_idx == APP_CALENDAR) return 0x587A46;
    if (app_idx == APP_TASKS) return 0x7A5E2E;
    if (app_idx == APP_CLIPBOARD) return 0x6C5C9A;
    if (app_idx == APP_NETWORK) return 0x3A6D9E;
    if (app_idx == APP_STORAGE) return 0x5A6A37;
    if (app_idx == APP_DIAGNOSTICS) return 0x8A5636;
    if (app_idx == APP_RELEASE_NOTES) return 0x365E8A;
    if (app_idx == APP_JOURNAL) return 0x7A4B4B;
    return 0x2A5F96;
}

static void draw_app_content_line(const rect_i* content, int line, const char* text, uint32_t color) {
    const int line_step = 12 * ui_text_scale(1);
    draw_text_clipped(content->x + 8, content->y + 8 + line * line_step, text, color, 1, content->w - 16);
}

static const char* theme_name(int idx) {
    if (idx == 0) {
        return "Classic";
    }
    if (idx == 1) {
        return "Olive";
    }
    if (idx == 2) {
        return "Twilight";
    }
    return "Custom";
}

static void draw_text_excerpt(const rect_i* content, int line_start, const char* text, int max_lines, uint32_t color) {
    if (text == NULL || max_lines <= 0) {
        return;
    }

    size_t i = 0;
    int line = 0;
    while (text[i] != '\0' && line < max_lines) {
        char row[104];
        size_t out = 0;
        while (text[i] != '\0' && text[i] != '\n' && out + 1 < sizeof(row)) {
            row[out++] = text[i++];
        }
        row[out] = '\0';
        if (text[i] == '\n') {
            ++i;
        }
        if (out > 0) {
            draw_app_content_line(content, line_start + line, row, color);
        }
        ++line;
    }
}

static void draw_file_preview(const rect_i* content,
                              const char* title,
                              const char* filename,
                              const char* empty_text,
                              const char* hint) {
    draw_app_content_line(content, 0, title, kPalette.text_primary);
    char buf[420];
    if (!fs_read(filename, buf, sizeof(buf))) {
        draw_app_content_line(content, 1, empty_text, kPalette.text_muted);
        if (hint != NULL && hint[0] != '\0') {
            draw_app_content_line(content, 2, hint, kPalette.text_muted);
        }
        return;
    }

    draw_text_excerpt(content, 1, buf, 4, kPalette.text_muted);
    if (hint != NULL && hint[0] != '\0') {
        draw_app_content_line(content, 6, hint, kPalette.text_muted);
    }
}

static bool file_entry_at(int index, char* name_out, size_t name_cap, size_t* out_size, fs_backend* out_backend) {
    if (index < 0 || name_out == NULL || name_cap == 0) {
        return false;
    }
    if (!fs_name_at((size_t)index, name_out, name_cap)) {
        return false;
    }
    if (out_size != NULL) {
        (void)fs_size_at((size_t)index, out_size);
    }
    if (out_backend != NULL) {
        (void)fs_backend_at((size_t)index, out_backend);
    }
    return true;
}

static rect_i files_row_rect(const rect_i* content, int row) {
    return rect_make(content->x + 8, content->y + 30 + row * kFileRowH, content->w - 16, kFileRowH - 1);
}

static rect_i settings_mouse_minus_rect(const rect_i* content) {
    return rect_make(content->x + 128, content->y + 26, 12, 12);
}

static rect_i settings_mouse_plus_rect(const rect_i* content) {
    return rect_make(content->x + 160, content->y + 26, 12, 12);
}

static rect_i settings_theme_prev_rect(const rect_i* content) {
    return rect_make(content->x + 128, content->y + 44, 12, 12);
}

static rect_i settings_theme_next_rect(const rect_i* content) {
    return rect_make(content->x + 160, content->y + 44, 12, 12);
}

static rect_i settings_resolution_toggle_rect(const rect_i* content) {
    return rect_make(content->x + 128, content->y + 62, 44, 12);
}

static rect_i settings_save_rect(const rect_i* content) {
    return rect_make(content->x + content->w - 74, content->y + 8, 64, 14);
}

static rect_i notes_save_rect(const rect_i* content) {
    return rect_make(content->x + content->w - 74, content->y + 8, 64, 14);
}

static rect_i notes_text_rect(const rect_i* content) {
    return rect_make(content->x + 8, content->y + 28, content->w - 16, content->h - 36);
}

static rect_i editor_save_rect(const rect_i* content) {
    return rect_make(content->x + content->w - 74, content->y + 8, 64, 14);
}

static rect_i editor_text_rect(const rect_i* content) {
    return rect_make(content->x + 8, content->y + 28, content->w - 16, content->h - 36);
}

static void draw_edit_buffer(const rect_i* text_rect, const char* text, size_t text_len, size_t cursor, bool show_cursor) {
    bb_draw_sunken_box(text_rect->x, text_rect->y, text_rect->w, text_rect->h, kPalette.log_bg);

    const rect_i grid = rect_inset(*text_rect, 4);
    int cols = terminal_cols_for_rect(grid);
    int rows = terminal_rows_for_rect(grid);
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;

    const size_t max_visible = (size_t)cols * (size_t)rows;
    size_t begin = 0;
    if (cursor > max_visible) {
        begin = cursor - max_visible;
    } else if (text_len > max_visible) {
        begin = text_len - max_visible;
    }

    int row = 0;
    int col = 0;
    int cursor_row = 0;
    int cursor_col = 0;
    bool cursor_set = false;

    for (size_t i = begin; i <= text_len && row < rows; ++i) {
        if (!cursor_set && i == cursor) {
            cursor_row = row;
            cursor_col = col;
            cursor_set = true;
        }

        if (i == text_len) {
            break;
        }

        const char c = text[i];
        if (c == '\n') {
            ++row;
            col = 0;
            continue;
        }
        if (col >= cols) {
            ++row;
            col = 0;
        }
        if (row >= rows) {
            break;
        }
        terminal_draw_cell_char(grid, col, row, c, kPalette.text_primary);
        ++col;
    }

    if (show_cursor && cursor_set && cursor_row < rows) {
        const int cx = grid.x + cursor_col * kTerminalCellW;
        const int cy = grid.y + cursor_row * kTerminalCellH;
        bb_fill_rect(cx + 1, cy + 2, 1, kTerminalCellH - 4, 0x003366);
    }
}

static void draw_files_content(const rect_i* content) {
    char msg[80];
    size_t idx = 0;
    msg[0] = '\0';
    buf_append_str(msg, sizeof(msg), &idx, "FILES ");
    buf_append_u32(msg, sizeof(msg), &idx, (uint32_t)fs_count());
    buf_append_str(msg, sizeof(msg), &idx, " (click to open)");
    draw_app_content_line(content, 0, msg, kPalette.text_primary);

    const int total = (int)fs_count();
    for (int row = 0; row < kFileRowsVisible; ++row) {
        const int file_idx = row;
        if (file_idx >= total) {
            break;
        }

        const rect_i rr = files_row_rect(content, row);
        char name[56];
        size_t size = 0;
        fs_backend backend = FS_BACKEND_RAM;
        if (!file_entry_at(file_idx, name, sizeof(name), &size, &backend)) {
            continue;
        }

        if (file_idx == s_files_selected) {
            bb_fill_rect(rr.x, rr.y, rr.w, rr.h, 0xD9E6F6);
        }

        char line[96];
        idx = 0;
        line[0] = '\0';
        buf_append_str(line, sizeof(line), &idx, name);
        buf_append_str(line, sizeof(line), &idx, "  ");
        buf_append_u32(line, sizeof(line), &idx, (uint32_t)size);
        buf_append_str(line, sizeof(line), &idx, "b ");
        buf_append_str(line, sizeof(line), &idx, backend == FS_BACKEND_RAM ? "ram" : "boot");
        draw_text_clipped(rr.x + 2, rr.y + 3, line, kPalette.text_primary, 1, rr.w - 4);
    }
}

static void draw_settings_content(const rect_i* content) {
    draw_app_content_line(content, 0, "Interactive settings (saved to settings.cfg)", kPalette.text_primary);

    char line[64];
    size_t idx = 0;
    line[0] = '\0';
    buf_append_str(line, sizeof(line), &idx, "Mouse speed: ");
    buf_append_u32(line, sizeof(line), &idx, (uint32_t)s_setting_mouse_speed);
    draw_app_content_line(content, 1, line, kPalette.text_muted);

    draw_app_content_line(content, 2, "Theme: Dark (locked)", kPalette.text_muted);

    draw_app_content_line(content, 3, s_settings_resolution_mode == 0 ? "Resolution mode: native" : "Resolution mode: large text", kPalette.text_muted);

    const rect_i m_minus = settings_mouse_minus_rect(content);
    const rect_i m_plus = settings_mouse_plus_rect(content);
    const rect_i res = settings_resolution_toggle_rect(content);
    const rect_i save = settings_save_rect(content);

    bb_draw_raised_box(m_minus.x, m_minus.y, m_minus.w, m_minus.h, kPalette.frame_bg);
    bb_draw_raised_box(m_plus.x, m_plus.y, m_plus.w, m_plus.h, kPalette.frame_bg);
    bb_draw_raised_box(res.x, res.y, res.w, res.h, kPalette.frame_bg);
    bb_draw_raised_box(save.x, save.y, save.w, save.h, kPalette.start_bg);

    draw_text_clipped(m_minus.x + 4, m_minus.y + 3, "-", kPalette.text_primary, 1, 8);
    draw_text_clipped(m_plus.x + 4, m_plus.y + 3, "+", kPalette.text_primary, 1, 8);
    draw_text_clipped(res.x + 6, res.y + 3, "TOGGLE", kPalette.text_primary, 1, res.w - 8);
    draw_text_clipped(save.x + 14, save.y + 3, "SAVE", kPalette.text_primary, 1, save.w - 10);
}

static void draw_perf_content(const rect_i* content) {
    draw_app_content_line(content, 0, "Realtime CPU/MEM from idle+ramdisk telemetry", kPalette.text_primary);

    const rect_i cpu = rect_make(content->x + 8, content->y + 24, content->w - 16, 42);
    const rect_i mem = rect_make(content->x + 8, content->y + 78, content->w - 16, 42);
    bb_draw_sunken_box(cpu.x, cpu.y, cpu.w, cpu.h, 0xF7F7F7);
    bb_draw_sunken_box(mem.x, mem.y, mem.w, mem.h, 0xF7F7F7);
    draw_text_clipped(cpu.x + 4, cpu.y + 4, "CPU", 0x203040, 1, 24);
    draw_text_clipped(mem.x + 4, mem.y + 4, "MEM", 0x203040, 1, 24);

    if (s_perf_hist_len > 0) {
        const int start = (s_perf_hist_head - s_perf_hist_len + kPerfHistory) % kPerfHistory;
        for (int i = 0; i < s_perf_hist_len; ++i) {
            const int idx = (start + i) % kPerfHistory;
            const int x = cpu.x + 30 + i;
            if (x >= cpu.x + cpu.w - 2) {
                break;
            }
            const int cpu_h = (int)((uint32_t)(cpu.h - 10) * (uint32_t)s_cpu_history[idx] / 100U);
            const int mem_h = (int)((uint32_t)(mem.h - 10) * (uint32_t)s_mem_history[idx] / 100U);
            bb_fill_rect(x, cpu.y + cpu.h - 4 - cpu_h, 1, cpu_h, 0x2E7A5E);
            bb_fill_rect(x, mem.y + mem.h - 4 - mem_h, 1, mem_h, 0x3C6CB5);
        }
    }
}

static void draw_notes_content(const rect_i* content) {
    draw_app_content_line(content, 0, s_notes_dirty ? "Notes (modified)" : "Notes", kPalette.text_primary);
    const rect_i save = notes_save_rect(content);
    const rect_i text_rect = notes_text_rect(content);
    bb_draw_raised_box(save.x, save.y, save.w, save.h, kPalette.start_bg);
    draw_text_clipped(save.x + 14, save.y + 3, "SAVE", kPalette.text_primary, 1, save.w - 8);
    draw_edit_buffer(&text_rect, s_notes_text, s_notes_len, s_notes_cursor, s_notes_focused && s_input_cursor_visible);
}

static void draw_button(const rect_i* r, const char* label, uint32_t fill, uint32_t text) {
    bb_draw_raised_box(r->x, r->y, r->w, r->h, fill);
    draw_text_clipped(r->x + 6, r->y + 4, label, text, 1, r->w - 12);
}

static void calc_reset(void) {
    s_calc_display[0] = '0';
    s_calc_display[1] = '\0';
    s_calc_accum = 0;
    s_calc_op = 0;
    s_calc_new_entry = true;
}

static int calc_parse_int(const char* txt) {
    int v = 0;
    bool neg = false;
    size_t i = 0;
    if (txt[0] == '-') {
        neg = true;
        ++i;
    }
    for (; txt[i] != '\0'; ++i) {
        if (txt[i] < '0' || txt[i] > '9') {
            continue;
        }
        v = v * 10 + (int)(txt[i] - '0');
    }
    return neg ? -v : v;
}

static void calc_set_display(const char* txt) {
    copy_str(s_calc_display, sizeof(s_calc_display), txt);
    s_calc_new_entry = true;
}

static void calc_apply_op(char op) {
    int cur = calc_parse_int(s_calc_display);
    if (s_calc_op == 0) {
        s_calc_accum = cur;
    } else {
        if (s_calc_op == '+') s_calc_accum += cur;
        else if (s_calc_op == '-') s_calc_accum -= cur;
        else if (s_calc_op == '*') s_calc_accum *= cur;
        else if (s_calc_op == '/') {
            if (cur == 0) {
                calc_set_display("ERR");
                s_calc_op = 0;
                return;
            }
            s_calc_accum /= cur;
        }
    }
    char buf[32];
    size_t idx = 0;
    buf[0] = '\0';
    buf_append_u32(buf, sizeof(buf), &idx, (uint32_t)(s_calc_accum < 0 ? -s_calc_accum : s_calc_accum));
    if (s_calc_accum < 0 && idx + 2 < sizeof(buf)) {
        for (size_t i = idx + 1; i-- > 0;) {
            buf[i + 1] = buf[i];
        }
        buf[0] = '-';
    }
    copy_str(s_calc_display, sizeof(s_calc_display), buf);
    s_calc_op = op;
    s_calc_new_entry = true;
}

static rect_i calc_button_rect(const rect_i* content, int row, int col) {
    const rect_i display = rect_make(content->x + 8, content->y + 8, content->w - 16, 36);
    const int bw = 52;
    const int bh = 36;
    const int gap = 6;
    const int base_x = content->x + 12;
    const int base_y = display.y + display.h + 10;
    return rect_make(base_x + col * (bw + gap), base_y + row * (bh + gap), bw, bh);
}

static bool handle_calculator_click(const rect_i* content) {
    static const char* labels[4][4] = {
        {"7","8","9","/"},
        {"4","5","6","*"},
        {"1","2","3","-"},
        {"0",".","=","+"},
    };
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            const rect_i btn = calc_button_rect(content, r, c);
            if (!rect_contains(btn, s_mouse_x, s_mouse_y)) {
                continue;
            }
            const char* label = labels[r][c];
            const char key = label[0];
            if (key >= '0' && key <= '9') {
                if (s_calc_new_entry) {
                    s_calc_display[0] = '\0';
                    s_calc_new_entry = false;
                }
                size_t len = cstr_len(s_calc_display);
                if (len < sizeof(s_calc_display) - 1) {
                    if (len == 1 && s_calc_display[0] == '0') {
                        s_calc_display[0] = key;
                        s_calc_display[1] = '\0';
                    } else {
                        s_calc_display[len] = key;
                        s_calc_display[len + 1] = '\0';
                    }
                }
            } else if (key == '.') {
                size_t len = cstr_len(s_calc_display);
                bool has_dot = false;
                for (size_t i = 0; i < len; ++i) {
                    if (s_calc_display[i] == '.') {
                        has_dot = true;
                        break;
                    }
                }
                if (!has_dot && len + 1 < sizeof(s_calc_display)) {
                    s_calc_display[len] = '.';
                    s_calc_display[len + 1] = '\0';
                    s_calc_new_entry = false;
                }
            } else if (key == '=') {
                calc_apply_op(0);
            } else {
                calc_apply_op(key);
            }
            request_redraw();
            return true;
        }
    }
    return false;
}

static void draw_calculator_content(const rect_i* content) {
    const rect_i display = rect_make(content->x + 8, content->y + 8, content->w - 16, 36);
    bb_draw_sunken_box(display.x, display.y, display.w, display.h, 0x1A2533);
    draw_text_clipped(display.x + 8, display.y + 10, s_calc_display, 0xE8EDF5, 1, display.w - 16);
    draw_text_clipped(display.x + display.w - 130, display.y + 10, "calc cmd also works", 0x9BB1C7, 1, 120);

    const int cols = 4;
    const int rows = 4;
    const int bw = 52;
    const int bh = 36;
    const int gap = 6;
    const int base_x = content->x + 12;
    const int base_y = display.y + display.h + 10;

    static const char* labels[4][4] = {
        {"7","8","9","/"},
        {"4","5","6","*"},
        {"1","2","3","-"},
        {"0",".","=","+"},
    };

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const rect_i btn = rect_make(base_x + c * (bw + gap), base_y + r * (bh + gap), bw, bh);
            const bool op = (labels[r][c][0] == '/' || labels[r][c][0] == '*' ||
                             labels[r][c][0] == '-' || labels[r][c][0] == '+');
            draw_button(&btn, labels[r][c], op ? 0x28415F : 0x233347, 0xE8EDF5);
        }
    }

    draw_app_content_line(content, 13, "Use terminal 'calc' command; UI mirrors keypad.", kPalette.text_muted);
}

static void draw_calendar_grid_cell(int x, int y, int w, int h, const char* text, bool header, bool highlight) {
    const uint32_t fill = header ? 0x1F2C3B : (highlight ? 0x25344A : 0x162130);
    const uint32_t border = 0x2E3D52;
    bb_draw_sunken_box(x, y, w, h, fill);
    bb_draw_border(x, y, w, h, border);
    draw_text_clipped(x + 6, y + 5, text, 0xE8EDF5, 1, w - 12);
}

static void draw_calendar_content(const rect_i* content) {
    draw_app_content_line(content, 0, "Calendar", kPalette.text_primary);
    draw_app_content_line(content, 1, pycoreos_build_stamp(), kPalette.text_muted);

    const int cell_w = 44;
    const int cell_h = 28;
    const int cols = 7;
    const int rows = 6;
    const int start_x = content->x + 6;
    const int start_y = content->y + 30;
    static const char* headers[7] = {"S","M","T","W","T","F","S"};

    for (int c = 0; c < cols; ++c) {
        draw_calendar_grid_cell(start_x + c * cell_w, start_y, cell_w, cell_h, headers[c], true, false);
    }

    int day = 1;
    for (int r = 1; r <= rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            char buf[4];
            buf[0] = '\0';
            if (day <= 30) {
                buf[0] = (char)('0' + (day / 10));
                if (buf[0] == '0') buf[0] = ' ';
                buf[1] = (char)('0' + (day % 10));
                buf[2] = '\0';
            } else {
                buf[0] = '\0';
            }
            const bool highlight = (day == 1 || day == 15);
            draw_calendar_grid_cell(start_x + c * cell_w, start_y + r * cell_h, cell_w, cell_h,
                                    buf[0] ? buf : "", false, highlight);
            ++day;
        }
    }

    draw_app_content_line(content, 10, "Planner: use journal/todo apps for notes & tasks.", kPalette.text_muted);
}

static void draw_resource_content(const rect_i* content) {
    draw_app_content_line(content, 0, "Resource Center", kPalette.text_primary);

    uint32_t cpu = 0;
    uint32_t mem = 0;
    if (s_perf_hist_len > 0) {
        const int last = (s_perf_hist_head + kPerfHistory - 1) % kPerfHistory;
        cpu = s_cpu_history[last];
        mem = s_mem_history[last];
    }
    const rect_i cpu_box = rect_make(content->x + 8, content->y + 18, (content->w - 24) / 2, 34);
    const rect_i mem_box = rect_make(cpu_box.x + cpu_box.w + 8, cpu_box.y, cpu_box.w, cpu_box.h);
    bb_draw_sunken_box(cpu_box.x, cpu_box.y, cpu_box.w, cpu_box.h, 0x1A2533);
    bb_draw_sunken_box(mem_box.x, mem_box.y, mem_box.w, mem_box.h, 0x1A2533);
    char line[32];
    size_t idx = 0;
    line[0] = '\0';
    buf_append_str(line, sizeof(line), &idx, "CPU ");
    buf_append_u32(line, sizeof(line), &idx, cpu);
    buf_append_char(line, sizeof(line), &idx, '%');
    draw_text_clipped(cpu_box.x + 6, cpu_box.y + 10, line, 0xE8EDF5, 1, cpu_box.w - 12);
    idx = 0; line[0] = '\0';
    buf_append_str(line, sizeof(line), &idx, "MEM ");
    buf_append_u32(line, sizeof(line), &idx, mem);
    buf_append_char(line, sizeof(line), &idx, '%');
    draw_text_clipped(mem_box.x + 6, mem_box.y + 10, line, 0xE8EDF5, 1, mem_box.w - 12);

    const rect_i perf_area = rect_make(content->x + 8, content->y + 60, content->w - 16, 110);
    bb_draw_sunken_box(perf_area.x, perf_area.y, perf_area.w, perf_area.h, 0x1A2533);
    if (s_perf_hist_len > 0) {
        const int start = (s_perf_hist_head - s_perf_hist_len + kPerfHistory) % kPerfHistory;
        for (int i = 0; i < s_perf_hist_len; ++i) {
            const int idx_hist = (start + i) % kPerfHistory;
            const int x = perf_area.x + 8 + i;
            if (x >= perf_area.x + perf_area.w - 2) break;
            const int h_cpu = (int)((uint32_t)(perf_area.h - 16) * (uint32_t)s_cpu_history[idx_hist] / 100U);
            const int h_mem = (int)((uint32_t)(perf_area.h - 16) * (uint32_t)s_mem_history[idx_hist] / 100U);
            bb_fill_rect(x, perf_area.y + perf_area.h - 6 - h_cpu, 1, h_cpu, 0x2E7A5E);
            bb_fill_rect(x, perf_area.y + perf_area.h - 6 - h_mem, 1, h_mem, 0x3C6CB5);
        }
    }

    char status[96];
    idx = 0;
    status[0] = '\0';
    buf_append_str(status, sizeof(status), &idx, net_stack_ready() ? "Net: ready" : "Net: offline");
    buf_append_str(status, sizeof(status), &idx, " | Files=");
    buf_append_u32(status, sizeof(status), &idx, (uint32_t)fs_count());
    buf_append_str(status, sizeof(status), &idx, " RAM ");
    buf_append_u32(status, sizeof(status), &idx, (uint32_t)fs_ramdisk_used());
    buf_append_str(status, sizeof(status), &idx, "/");
    buf_append_u32(status, sizeof(status), &idx, (uint32_t)fs_ramdisk_capacity());
    draw_app_content_line(content, 11, status, kPalette.text_muted);
    draw_app_content_line(content, 12, "For full graphs open PERFORMANCE.", kPalette.text_muted);
}

static void draw_editor_content(const rect_i* content) {
    char title[80];
    size_t idx = 0;
    title[0] = '\0';
    buf_append_str(title, sizeof(title), &idx, "Editor: ");
    buf_append_str(title, sizeof(title), &idx, s_editor_filename[0] ? s_editor_filename : "(none)");
    if (s_editor_dirty) {
        buf_append_str(title, sizeof(title), &idx, " *");
    }
    draw_app_content_line(content, 0, title, kPalette.text_primary);

    const rect_i save = editor_save_rect(content);
    const rect_i text_rect = editor_text_rect(content);
    bb_draw_raised_box(save.x, save.y, save.w, save.h, kPalette.start_bg);
    draw_text_clipped(save.x + 14, save.y + 3, "SAVE", kPalette.text_primary, 1, save.w - 8);
    draw_edit_buffer(&text_rect, s_editor_text, s_editor_len, s_editor_cursor, s_editor_focused && s_input_cursor_visible);
}

static void draw_app_window_content(int app_idx, const rect_i* content) {
    if (app_idx == APP_HELP) {
        draw_app_content_line(content, 0, "Quality-of-life command packs:", kPalette.text_primary);
        draw_app_content_line(content, 1, "system: sysinfo meminfo netinfo history date time uname", kPalette.text_muted);
        draw_app_content_line(content, 2, "files: find head tail grep wc clip todo journal", kPalette.text_muted);
        draw_app_content_line(content, 3, "desktop: apps open theme resmode calc", kPalette.text_muted);
        draw_app_content_line(content, 4, "legacy: ls cat touch write append cp mv savefs doom", kPalette.text_muted);
        return;
    }

    if (app_idx == APP_FILES) {
        draw_files_content(content);
        return;
    }

    if (app_idx == APP_SYSTEM) {
        char line1[64];
        char line2[64];
        char line3[72];
        size_t idx = 0;
        line1[0] = '\0';
        buf_append_str(line1, sizeof(line1), &idx, "Display ");
        buf_append_u32(line1, sizeof(line1), &idx, (uint32_t)kScreenWidth);
        buf_append_char(line1, sizeof(line1), &idx, 'x');
        buf_append_u32(line1, sizeof(line1), &idx, (uint32_t)kScreenHeight);

        idx = 0;
        line2[0] = '\0';
        buf_append_str(line2, sizeof(line2), &idx, "Uptime ");
        buf_append_u32(line2, sizeof(line2), &idx, s_ticks / kTicksPerSecondEstimate);
        buf_append_char(line2, sizeof(line2), &idx, 's');

        idx = 0;
        line3[0] = '\0';
        buf_append_str(line3, sizeof(line3), &idx, "Theme ");
        buf_append_str(line3, sizeof(line3), &idx, theme_name(s_theme_index));
        buf_append_str(line3, sizeof(line3), &idx, " / ");
        buf_append_str(line3, sizeof(line3), &idx, s_settings_resolution_mode == 0 ? "native" : "large");

        draw_app_content_line(content, 0, line1, kPalette.text_primary);
        draw_app_content_line(content, 1, line2, kPalette.text_muted);
        draw_app_content_line(content, 2, line3, kPalette.text_muted);
        draw_app_content_line(content, 3, pycoreos_version(), kPalette.text_muted);
        draw_app_content_line(content, 4, "Lead OSDev Johan Joseph", kPalette.text_muted);
        return;
    }

    if (app_idx == APP_MOUSE) {
        char line1[64];
        char line2[64];
        size_t idx = 0;
        line1[0] = '\0';
        buf_append_str(line1, sizeof(line1), &idx, "Pointer X=");
        buf_append_u32(line1, sizeof(line1), &idx, (uint32_t)s_mouse_x);
        buf_append_str(line1, sizeof(line1), &idx, " Y=");
        buf_append_u32(line1, sizeof(line1), &idx, (uint32_t)s_mouse_y);

        idx = 0;
        line2[0] = '\0';
        buf_append_str(line2, sizeof(line2), &idx, "Buttons L");
        buf_append_char(line2, sizeof(line2), &idx, s_mouse_left ? '1' : '0');
        buf_append_str(line2, sizeof(line2), &idx, " R");
        buf_append_char(line2, sizeof(line2), &idx, s_mouse_right ? '1' : '0');
        buf_append_str(line2, sizeof(line2), &idx, " M");
        buf_append_char(line2, sizeof(line2), &idx, s_mouse_middle ? '1' : '0');

        draw_app_content_line(content, 0, line1, kPalette.text_primary);
        draw_app_content_line(content, 1, line2, kPalette.text_muted);
        draw_app_content_line(content, 2, "Cursor style: classic sprite set", kPalette.text_muted);
        return;
    }

    if (app_idx == APP_SETTINGS) {
        draw_settings_content(content);
        return;
    }

    if (app_idx == APP_PERFORMANCE) {
        draw_perf_content(content);
        return;
    }

    if (app_idx == APP_NOTES) {
        draw_notes_content(content);
        return;
    }

    if (app_idx == APP_ABOUT) {
        draw_app_content_line(content, 0, "PyCoreOS classic desktop shell", kPalette.text_primary);
        draw_app_content_line(content, 1, pycoreos_version(), kPalette.text_muted);
        draw_app_content_line(content, 2, pycoreos_channel(), kPalette.text_muted);
        draw_app_content_line(content, 3, "32-bit educational OS project", kPalette.text_muted);
        draw_app_content_line(content, 4, "WM, CLI, filesystem, networking, doom", kPalette.text_muted);
        return;
    }

    if (app_idx == APP_CREDITS) {
        draw_app_content_line(content, 0, "Lead OSDev", kPalette.text_primary);
        draw_app_content_line(content, 1, "JOHAN JOSEPH", 0x0A246A);
        draw_app_content_line(content, 2, "Desktop and core UX direction", kPalette.text_muted);
        return;
    }

    if (app_idx == APP_TIPS) {
        draw_app_content_line(content, 0, "Drag title bars to move windows", kPalette.text_primary);
        draw_app_content_line(content, 1, "Resize terminal from edges/corners", kPalette.text_muted);
        draw_app_content_line(content, 2, "Use Start + Desktop logos for apps", kPalette.text_muted);
        return;
    }

    if (app_idx == APP_EDITOR) {
        draw_editor_content(content);
        return;
    }

    if (app_idx == APP_DOOM) {
        draw_app_content_line(content, 0, "Doom bridge is available.", kPalette.text_primary);
        draw_app_content_line(content, 1, "Run command: doom", kPalette.text_muted);
        draw_app_content_line(content, 2, "Window stays open for quick access.", kPalette.text_muted);
        return;
    }

    if (app_idx == APP_CALCULATOR) {
        draw_calculator_content(content);
        return;
    }

    if (app_idx == APP_CLOCK) {
        char line[72];
        size_t idx = 0;
        line[0] = '\0';
        buf_append_str(line, sizeof(line), &idx, "Uptime ");
        buf_append_u32(line, sizeof(line), &idx, s_ticks / kTicksPerSecondEstimate);
        buf_append_str(line, sizeof(line), &idx, "s  ticks=");
        buf_append_u32(line, sizeof(line), &idx, s_ticks);
        draw_app_content_line(content, 0, "Session Clock", kPalette.text_primary);
        draw_app_content_line(content, 1, line, kPalette.text_muted);
        draw_app_content_line(content, 2, "Use 'time' or 'date' in terminal for text output.", kPalette.text_muted);
        return;
    }

    if (app_idx == APP_CALENDAR) {
        draw_calendar_content(content);
        return;
    }

    if (app_idx == APP_TASKS) {
        draw_file_preview(content, "Task board (todo.txt)", "todo.txt",
                          "No task list yet.", "Use: todo add <text>");
        return;
    }

    if (app_idx == APP_CLIPBOARD) {
        draw_file_preview(content, "Clipboard (clipboard.txt)", "clipboard.txt",
                          "Clipboard is empty.", "Use: clip set <text>");
        return;
    }

    if (app_idx == APP_NETWORK) {
        draw_app_content_line(content, 0, "Network panel", kPalette.text_primary);
        draw_app_content_line(content, 1, net_stack_ready() ? "RTL8139 stack: ready" : "RTL8139 stack: unavailable",
                              kPalette.text_muted);
        draw_app_content_line(content, 2, "Send test packet with: ping 1.1.1.1", kPalette.text_muted);
        draw_app_content_line(content, 3, "More details: netinfo", kPalette.text_muted);
        return;
    }

    if (app_idx == APP_STORAGE) {
        char line1[80];
        char line2[80];
        size_t idx = 0;
        line1[0] = '\0';
        buf_append_str(line1, sizeof(line1), &idx, "Files: ");
        buf_append_u32(line1, sizeof(line1), &idx, (uint32_t)fs_count());
        buf_append_str(line1, sizeof(line1), &idx, "  RAM used: ");
        buf_append_u32(line1, sizeof(line1), &idx, (uint32_t)fs_ramdisk_used());
        idx = 0;
        line2[0] = '\0';
        buf_append_str(line2, sizeof(line2), &idx, "RAM cap: ");
        buf_append_u32(line2, sizeof(line2), &idx, (uint32_t)fs_ramdisk_capacity());
        draw_app_content_line(content, 0, "Storage overview", kPalette.text_primary);
        draw_app_content_line(content, 1, line1, kPalette.text_muted);
        draw_app_content_line(content, 2, line2, kPalette.text_muted);
        draw_app_content_line(content, 3, "Use savefs/loadfs for persistence snapshots.", kPalette.text_muted);
        return;
    }

    if (app_idx == APP_DIAGNOSTICS) {
        char line1[80];
        char line2[96];
        size_t idx = 0;
        line1[0] = '\0';
        buf_append_str(line1, sizeof(line1), &idx, "No sound subsystem");
        idx = 0;
        line2[0] = '\0';
        buf_append_str(line2, sizeof(line2), &idx, "Mouse x=");
        buf_append_u32(line2, sizeof(line2), &idx, (uint32_t)s_mouse_x);
        buf_append_str(line2, sizeof(line2), &idx, " y=");
        buf_append_u32(line2, sizeof(line2), &idx, (uint32_t)s_mouse_y);
        draw_app_content_line(content, 0, "Diagnostics summary", kPalette.text_primary);
        draw_app_content_line(content, 1, line1, kPalette.text_muted);
        draw_app_content_line(content, 2, line2, kPalette.text_muted);
        draw_app_content_line(content, 3, net_stack_ready() ? "Network stack online" : "Network stack offline",
                              kPalette.text_muted);
        return;
    }

    if (app_idx == APP_MONITOR) {
        draw_resource_content(content);
        return;
    }

    if (app_idx == APP_TERMINAL_GUIDE) {
        draw_app_content_line(content, 0, "Terminal guide", kPalette.text_primary);
        draw_app_content_line(content, 1, "help | apps | open <app> | history", kPalette.text_muted);
        draw_app_content_line(content, 2, "find/head/tail/grep/wc for text workflows", kPalette.text_muted);
        draw_app_content_line(content, 3, "clip/todo/journal commands store quick notes", kPalette.text_muted);
        return;
    }

    if (app_idx == APP_WALLPAPER) {
        draw_app_content_line(content, 0, "Wallpaper loader", kPalette.text_primary);
        draw_app_content_line(content, 1, s_wallpaper_loaded ? "wallpaper.bmp/tga active" : "No wallpaper file loaded",
                              kPalette.text_muted);
        draw_app_content_line(content, 2, "Supported files: wallpaper.bmp or wallpaper.tga", kPalette.text_muted);

        const rect_i preview = rect_make(content->x + 10, content->y + 48, 180, 102);
        bb_draw_sunken_box(preview.x, preview.y, preview.w, preview.h, 0x0F172A);
        if (s_wallpaper_loaded) {
            for (int py = 0; py < preview.h - 4; ++py) {
                const int sy = (py * kScreenHeight) / (preview.h - 4);
                for (int px = 0; px < preview.w - 4; ++px) {
                    const int sx = (px * kScreenWidth) / (preview.w - 4);
                    const uint32_t c = s_wallpaper[(size_t)sy * kBackbufferMaxW + (size_t)sx];
                    bb_put_pixel(preview.x + 2 + px, preview.y + 2 + py, c);
                }
            }
        } else {
            draw_text_clipped(preview.x + 8, preview.y + 42, "No Preview", 0xC7D2E0, 1, preview.w - 16);
        }
        return;
    }

    if (app_idx == APP_SHORTCUTS) {
        draw_app_content_line(content, 0, "Shortcuts", kPalette.text_primary);
        draw_app_content_line(content, 1, "Taskbar quick buttons: HELP / FILES / DOOM", kPalette.text_muted);
        draw_app_content_line(content, 2, "Desktop icons launch core apps", kPalette.text_muted);
        draw_app_content_line(content, 3, "Use Start menu for full app catalog", kPalette.text_muted);
        return;
    }

    if (app_idx == APP_TROUBLESHOOT) {
        draw_app_content_line(content, 0, "Troubleshooting checklist", kPalette.text_primary);
        draw_app_content_line(content, 1, "1) run: sysinfo", kPalette.text_muted);
        draw_app_content_line(content, 2, "2) run: fsinfo + savefs", kPalette.text_muted);
        draw_app_content_line(content, 3, "3) run: netinfo + ping <ip>", kPalette.text_muted);
        draw_app_content_line(content, 4, "4) run: betareport", kPalette.text_muted);
        return;
    }

    if (app_idx == APP_RELEASE_NOTES) {
        draw_file_preview(content, "Release notes (CHANGELOG.md)", "CHANGELOG.md",
                          "No changelog file found.", "Use editor/files to inspect full notes.");
        return;
    }

    if (app_idx == APP_ROADMAP) {
        draw_file_preview(content, "Roadmap (updates.md)", "updates.md",
                          "No roadmap file found.", "Tracks upcoming PyCoreOS milestones.");
        return;
    }

    if (app_idx == APP_JOURNAL) {
        draw_file_preview(content, "Journal (journal.txt)", "journal.txt",
                          "No journal entries yet.", "Use: journal add <text>");
        return;
    }

    if (app_idx == APP_TODO) {
        draw_file_preview(content, "Todo list (todo.txt)", "todo.txt",
                          "Todo list is empty.", "Use: todo add <text>");
        return;
    }

    if (app_idx == APP_PACKAGES) {
        draw_app_content_line(content, 0, "Boot module packages", kPalette.text_primary);
        int line = 1;
        const int total = (int)fs_count();
        for (int i = 0; i < total && line < 6; ++i) {
            fs_backend backend = FS_BACKEND_RAM;
            if (!fs_backend_at((size_t)i, &backend) || backend != FS_BACKEND_BOOT_MODULE) {
                continue;
            }
            char name[64];
            if (!fs_name_at((size_t)i, name, sizeof(name))) {
                continue;
            }
            draw_app_content_line(content, line++, name, kPalette.text_muted);
        }
        if (line == 1) {
            draw_app_content_line(content, 1, "No external packages detected.", kPalette.text_muted);
        }
        return;
    }

    if (app_idx == APP_SNAPSHOTS) {
        draw_app_content_line(content, 0, "Snapshot workflow", kPalette.text_primary);
        draw_app_content_line(content, 1, "savefs: write RAM filesystem image", kPalette.text_muted);
        draw_app_content_line(content, 2, "loadfs: restore saved image", kPalette.text_muted);
        draw_app_content_line(content, 3, "betareport: capture diagnostics snapshot", kPalette.text_muted);
        return;
    }

    if (app_idx == APP_LAUNCHER) {
        draw_app_content_line(content, 0, "Launcher aliases", kPalette.text_primary);
        draw_app_content_line(content, 1, "open calc / open net / open roadmap", kPalette.text_muted);
        draw_app_content_line(content, 2, "open journal / open todo / open release", kPalette.text_muted);
        draw_app_content_line(content, 3, "Use command: apps", kPalette.text_muted);
        return;
    }

    draw_app_content_line(content, 0, "App content unavailable.", kPalette.text_primary);
}

static void draw_title_bar_button(rect_i r, uint32_t base_color, bool hover, bool pressed) {
    uint32_t fill = base_color;
    if (pressed) {
        fill = color_lerp(base_color, 0x0A0A0A, 80U);
    } else if (hover) {
        fill = color_lerp(base_color, 0xFFFFFF, 30U);
    }
    bb_draw_raised_box(r.x, r.y, r.w, r.h, fill);
}

static void draw_single_app_window(int app_idx) {
    const rect_i window = app_window_rect(app_idx);
    const rect_i title = app_window_title_rect(app_idx);
    const rect_i minr = app_window_min_rect(app_idx);
    const rect_i maxr = app_window_max_rect(app_idx);
    const rect_i close = app_window_close_rect(app_idx);
    const rect_i resize = app_window_resize_rect(app_idx);
    const rect_i content = app_window_content_rect(app_idx);

    const bool active = (s_active_app_idx == app_idx);
    uint32_t top = app_window_accent_top(app_idx);
    uint32_t bottom = color_lerp(top, 0x0A246A, 118);
    if (!active) {
        top = color_lerp(top, 0x0A0A12, 100U);
        bottom = color_lerp(bottom, 0x0A0A12, 100U);
    }

    bb_draw_soft_panel(window.x, window.y, window.w, window.h, 8, kPalette.frame_bg, 0x253B55);
    bb_draw_vgradient(title.x, title.y, title.w, title.h, top, bottom);
    bb_draw_border(title.x, title.y, title.w, title.h, 0x06163F);

    const rect_i badge = rect_make(title.x + 6, title.y + 2, 16, 16);
    draw_app_icon_badge(&badge, app_idx, false);
    draw_text_clipped(title.x + 26, title.y + 4, kAppWindowTitles[app_idx], kPalette.title_text, 1, title.w - 76);

    const bool min_hover = rect_contains(minr, s_mouse_x, s_mouse_y);
    const bool max_hover = rect_contains(maxr, s_mouse_x, s_mouse_y);
    const bool close_hover = rect_contains(close, s_mouse_x, s_mouse_y);
    const bool min_pressed = (s_app_btn_pressed_idx == app_idx && s_app_btn_pressed_btn == 0 && s_mouse_left);
    const bool max_pressed = (s_app_btn_pressed_idx == app_idx && s_app_btn_pressed_btn == 1 && s_mouse_left);
    const bool close_pressed = (s_app_btn_pressed_idx == app_idx && s_app_btn_pressed_btn == 2 && s_mouse_left);

    draw_title_bar_button(minr, kPalette.frame_bg, min_hover, min_pressed);
    draw_title_bar_button(maxr, kPalette.frame_bg, max_hover, max_pressed);
    draw_title_bar_button(close, kPalette.frame_bg, close_hover, close_pressed);

    bb_fill_rect(minr.x + 4, minr.y + 8, 8, 1, 0x202020);
    bb_draw_border(maxr.x + 3, maxr.y + 3, 10, 10, 0x38485C);
    for (int i = 0; i < 8; ++i) {
        bb_fill_rect(close.x + 3 + i, close.y + 3 + i, 1, 1, 0x7A0000);
        bb_fill_rect(close.x + 12 - i, close.y + 3 + i, 1, 1, 0x7A0000);
    }

    bb_draw_sunken_box(content.x, content.y, content.w, content.h, kPalette.log_bg);
    draw_app_window_content(app_idx, &content);

    if (!s_app_windows[app_idx].maximized) {
        bb_draw_raised_box(resize.x, resize.y, resize.w, resize.h, kPalette.frame_bg);
        bb_fill_rect(resize.x + 7, resize.y + 9, 2, 1, 0x38485C);
        bb_fill_rect(resize.x + 5, resize.y + 7, 2, 1, 0x38485C);
        bb_fill_rect(resize.x + 3, resize.y + 5, 2, 1, 0x38485C);
    }
}

static void draw_app_windows(const ui_layout* l) {
    for (int z = 0; z < kAppWindowCount; ++z) {
        const int app_idx = s_app_z_order[z];
        if (!s_app_windows[app_idx].open || s_app_windows[app_idx].minimized) {
            continue;
        }
        if (!s_app_windows[app_idx].maximized) {
            clamp_app_window_to_desktop(&s_app_windows[app_idx], l);
        }
        draw_single_app_window(app_idx);
    }
}

static void draw_background(const ui_layout* l) {
    if (s_wallpaper_loaded) {
        for (int y = 0; y < l->taskbar.y; ++y) {
            for (int x = 0; x < l->screen_w; ++x) {
                s_draw_target[(size_t)y * kBackbufferMaxW + (size_t)x] =
                    s_wallpaper[(size_t)y * kBackbufferMaxW + (size_t)x];
            }
        }
    } else {
        bb_draw_vgradient(0, 0, l->screen_w, l->taskbar.y, color_lerp(s_theme_desktop_bg, 0xFFFFFF, 18U),
                          color_lerp(s_theme_desktop_bg, 0x101820, 24U));
        for (int y = 0; y < l->taskbar.y; y += 24) {
            bb_blend_rect(0, y, l->screen_w, 1, 0xFFFFFF, 20U);
        }
        bb_fill_round_rect_alpha(36, 30, 190, 92, 18, 0xFFFFFF, 26U);
        bb_fill_round_rect_alpha(l->screen_w - 250, 70, 210, 110, 24, 0xFFFFFF, 18U);
    }
}

static int terminal_log_max_scroll(const ui_layout* l) {
    const rect_i log_grid = rect_inset(l->log_box, 4);
    int rows = terminal_rows_for_rect(log_grid);
    if (rows < 1) {
        rows = 1;
    }
    if (s_log_count <= (size_t)rows) {
        return 0;
    }
    return (int)(s_log_count - (size_t)rows);
}

static void draw_status_text(const rect_i* status) {
    char text[112];
    size_t idx = 0;
    text[0] = '\0';

    buf_append_str(text, sizeof(text), &idx, "WM: ");
    if (s_terminal_window.minimized) {
        buf_append_str(text, sizeof(text), &idx, "MINIMIZED ");
    } else if (s_terminal_window.maximized) {
        buf_append_str(text, sizeof(text), &idx, "MAXIMIZED ");
    } else if (s_terminal_window.resizing) {
        buf_append_str(text, sizeof(text), &idx, "RESIZING ");
    } else {
        buf_append_str(text, sizeof(text), &idx, "ACTIVE ");
    }
    buf_append_str(text, sizeof(text), &idx, "X=");
    buf_append_u32(text, sizeof(text), &idx, (uint32_t)s_mouse_x);
    buf_append_str(text, sizeof(text), &idx, " Y=");
    buf_append_u32(text, sizeof(text), &idx, (uint32_t)s_mouse_y);
    buf_append_str(text, sizeof(text), &idx, " L");
    buf_append_char(text, sizeof(text), &idx, s_mouse_left ? '1' : '0');
    buf_append_str(text, sizeof(text), &idx, " R");
    buf_append_char(text, sizeof(text), &idx, s_mouse_right ? '1' : '0');
    buf_append_str(text, sizeof(text), &idx, " M");
    buf_append_char(text, sizeof(text), &idx, s_mouse_middle ? '1' : '0');
    buf_append_str(text, sizeof(text), &idx, " W");
    if (s_mouse_wheel_accum < 0) {
        buf_append_char(text, sizeof(text), &idx, '-');
        buf_append_u32(text, sizeof(text), &idx, (uint32_t)(-s_mouse_wheel_accum));
    } else {
        buf_append_u32(text, sizeof(text), &idx, (uint32_t)s_mouse_wheel_accum);
    }
    buf_append_str(text, sizeof(text), &idx, " GPU FASTBLIT");

    draw_text_clipped(status->x + 8, status->y + 8, text, kPalette.text_muted, 1, status->w - 16);
}

static void draw_terminal_window_chrome(const ui_layout* l) {
    if (s_terminal_window.minimized) {
        return;
    }

    bb_draw_soft_panel(l->window.x, l->window.y, l->window.w, l->window.h, 10, kPalette.frame_bg, 0x2D3F58);

    uint32_t title_top = kPalette.title_top;
    uint32_t title_bottom = kPalette.title_bottom;
    if (!s_active_is_terminal) {
        title_top = color_lerp(title_top, 0x0A0A12, 90U);
        title_bottom = color_lerp(title_bottom, 0x0A0A12, 90U);
    }
    bb_draw_vgradient(l->titlebar.x, l->titlebar.y, l->titlebar.w, l->titlebar.h, title_top, title_bottom);
    bb_draw_border(l->titlebar.x, l->titlebar.y, l->titlebar.w, l->titlebar.h, 0x06163F);
    draw_text_clipped(l->titlebar.x + 6, l->titlebar.y + 4, "PYCOREOS TERMINAL", kPalette.title_text, 2, l->titlebar.w - 140);
    draw_text_clipped(l->titlebar.x + l->titlebar.w - 88, l->titlebar.y + 4,
                      session_title_label(), kPalette.title_subtext, 1, 80);

    const bool min_hover = rect_contains(l->btn_min, s_mouse_x, s_mouse_y);
    const bool max_hover = rect_contains(l->btn_max, s_mouse_x, s_mouse_y);
    const bool close_hover = rect_contains(l->btn_close, s_mouse_x, s_mouse_y);
    const bool min_pressed = (s_terminal_btn_pressed == 1 && s_mouse_left);
    const bool max_pressed = (s_terminal_btn_pressed == 2 && s_mouse_left);
    const bool close_pressed = (s_terminal_btn_pressed == 3 && s_mouse_left);

    draw_title_bar_button(l->btn_min, kPalette.frame_bg, min_hover, min_pressed);
    draw_title_bar_button(l->btn_max, kPalette.frame_bg, max_hover, max_pressed);
    draw_title_bar_button(l->btn_close, kPalette.frame_bg, close_hover, close_pressed);

    bb_fill_rect(l->btn_min.x + 4, l->btn_min.y + 8, 8, 1, 0x0A0A0A);
    if (s_terminal_window.maximized) {
        bb_fill_rect(l->btn_max.x + 5, l->btn_max.y + 5, 6, 4, 0x0A0A0A);
        bb_fill_rect(l->btn_max.x + 7, l->btn_max.y + 3, 6, 4, 0x0A0A0A);
    } else {
        bb_draw_border(l->btn_max.x + 4, l->btn_max.y + 4, 8, 8, 0x38485C);
    }
    for (int i = 0; i < 8; ++i) {
        bb_fill_rect(l->btn_close.x + 4 + i, l->btn_close.y + 4 + i, 1, 1, 0x7A0000);
        bb_fill_rect(l->btn_close.x + 11 - i, l->btn_close.y + 4 + i, 1, 1, 0x7A0000);
    }

    bb_draw_sunken_box(l->log_box.x, l->log_box.y, l->log_box.w, l->log_box.h, kPalette.log_bg);
    bb_draw_sunken_box(l->input_box.x, l->input_box.y, l->input_box.w, l->input_box.h, kPalette.input_bg);
    bb_draw_raised_box(l->status_box.x, l->status_box.y, l->status_box.w, l->status_box.h, kPalette.status_bg);
}

static void draw_terminal_window_dynamic(const ui_layout* l) {
    if (s_terminal_window.minimized) {
        return;
    }

    const rect_i log_grid = rect_inset(l->log_box, 4);
    int log_cols = terminal_cols_for_rect(log_grid);
    int log_rows = terminal_rows_for_rect(log_grid);
    if (log_cols < 1) {
        log_cols = 1;
    }
    if (log_rows < 1) {
        log_rows = 1;
    }

    const int max_scroll = terminal_log_max_scroll(l);
    s_log_scroll = clamp_i32(s_log_scroll, 0, max_scroll);
    size_t start = 0;
    if (s_log_count > (size_t)log_rows) {
        start = s_log_count - (size_t)log_rows - (size_t)s_log_scroll;
    }

    for (int row = 0; row < log_rows; ++row) {
        const size_t idx = start + (size_t)row;
        if (idx >= s_log_count) {
            break;
        }
        terminal_draw_text_line(log_grid, row, 0, s_log[idx], kPalette.text_primary, log_cols);
    }

    const rect_i input_grid = rect_inset(l->input_box, 4);
    int input_cols = terminal_cols_for_rect(input_grid);
    if (input_cols < 1) {
        input_cols = 1;
    }

    const char* prompt = terminal_prompt_text();
    const int prompt_cols = (int)cstr_len(prompt);
    terminal_draw_text_line(input_grid, 0, 0, prompt, kPalette.text_muted, input_cols);

    int input_col_start = prompt_cols;
    if (input_col_start >= input_cols) {
        input_col_start = input_cols - 1;
    }
    int visible_input_cols = input_cols - input_col_start;
    if (visible_input_cols < 1) {
        visible_input_cols = 1;
    }

    size_t visible_begin = 0;
    if (s_input_len > (size_t)(visible_input_cols - 1)) {
        visible_begin = s_input_len - (size_t)(visible_input_cols - 1);
    }

    terminal_draw_text_line(input_grid, 0, input_col_start, s_input_line + visible_begin, kPalette.text_primary, visible_input_cols);

    if (s_input_cursor_visible) {
        int cursor_col = input_col_start + (int)(s_input_len - visible_begin);
        if (cursor_col >= input_cols) {
            cursor_col = input_cols - 1;
        }
        if (cursor_col >= 0) {
            const int cx = input_grid.x + cursor_col * kTerminalCellW;
            const int cy = input_grid.y;
            bb_fill_rect(cx + 1, cy + 2, 1, kTerminalCellH - 4, kPalette.text_primary);
        }
    }

    draw_status_text(&l->status_box);
}

static void draw_taskbar_chrome(const ui_layout* l) {
    bb_fill_round_rect(l->taskbar.x, l->taskbar.y, l->taskbar.w, l->taskbar.h, 6, s_theme_taskbar_bg);
    bb_blend_rect(l->taskbar.x, l->taskbar.y, l->taskbar.w, 2, 0xFFFFFF, 64U);

    uint32_t start_fill = s_theme_start_bg;
    if (s_session_user == SESSION_USER_GUEST) {
        start_fill = 0x2B7250;
    }
    bb_fill_round_rect(l->start_button.x, l->start_button.y, l->start_button.w, l->start_button.h, 6, start_fill);
    bb_draw_border(l->start_button.x, l->start_button.y, l->start_button.w, l->start_button.h, 0x536637);
    draw_text_clipped(l->start_button.x + 12, l->start_button.y + 9, "START", kPalette.text_primary, 1, l->start_button.w - 20);

    bb_fill_round_rect(l->task_terminal_button.x, l->task_terminal_button.y, l->task_terminal_button.w, l->task_terminal_button.h, 6, kPalette.frame_bg);
    bb_fill_round_rect(l->quick_help_button.x, l->quick_help_button.y, l->quick_help_button.w, l->quick_help_button.h, 6, kPalette.frame_bg);
    bb_fill_round_rect(l->quick_files_button.x, l->quick_files_button.y, l->quick_files_button.w, l->quick_files_button.h, 6, kPalette.frame_bg);
    bb_fill_round_rect(l->quick_doom_button.x, l->quick_doom_button.y, l->quick_doom_button.w, l->quick_doom_button.h, 6, kPalette.frame_bg);
    bb_draw_sunken_box(l->clock_box.x, l->clock_box.y, l->clock_box.w, l->clock_box.h, kPalette.frame_bg);
}

static bool app_task_button_rect(const ui_layout* l, int slot, rect_i* out_rect, int* out_app_idx) {
    if (slot < 0) {
        return false;
    }

    const int x0 = l->quick_doom_button.x + l->quick_doom_button.w + kAppTaskButtonGap;
    const int y0 = l->taskbar.y + 5;
    const int max_w = l->clock_box.x - 6;
    int open_seen = 0;

    for (int i = 0; i < kAppWindowCount; ++i) {
        const int app_idx = s_app_z_order[i];
        if (!s_app_windows[app_idx].open) {
            continue;
        }
        if (open_seen == slot) {
            const int x = x0 + slot * (kAppTaskButtonW + kAppTaskButtonGap);
            if (x + kAppTaskButtonW > max_w) {
                return false;
            }
            if (out_rect != NULL) {
                *out_rect = rect_make(x, y0, kAppTaskButtonW, 24);
            }
            if (out_app_idx != NULL) {
                *out_app_idx = app_idx;
            }
            return true;
        }
        ++open_seen;
    }
    return false;
}

static void draw_taskbar_dynamic(const ui_layout* l) {
    if (rect_contains(l->quick_help_button, s_mouse_x, s_mouse_y)) {
        bb_fill_round_rect_alpha(l->quick_help_button.x + 1, l->quick_help_button.y + 1,
                                 l->quick_help_button.w - 2, l->quick_help_button.h - 2, 5,
                                 0xDCE8F5, 96U + hover_anim_t(3) / 3U);
    }
    if (rect_contains(l->quick_files_button, s_mouse_x, s_mouse_y)) {
        bb_fill_round_rect_alpha(l->quick_files_button.x + 1, l->quick_files_button.y + 1,
                                 l->quick_files_button.w - 2, l->quick_files_button.h - 2, 5,
                                 0xDFF1E2, 96U + hover_anim_t(11) / 3U);
    }
    if (rect_contains(l->quick_doom_button, s_mouse_x, s_mouse_y)) {
        bb_fill_round_rect_alpha(l->quick_doom_button.x + 1, l->quick_doom_button.y + 1,
                                 l->quick_doom_button.w - 2, l->quick_doom_button.h - 2, 5,
                                 0xF3DFDF, 96U + hover_anim_t(19) / 3U);
    }

    draw_text_clipped(l->task_terminal_button.x + 8, l->task_terminal_button.y + 9,
                      s_terminal_window.minimized ? "TERMINAL (MIN)" : "TERMINAL",
                      kPalette.text_primary, 1, l->task_terminal_button.w - 12);
    draw_text_clipped(l->quick_help_button.x + 12, l->quick_help_button.y + 9,
                      "HELP", kPalette.text_primary, 1, l->quick_help_button.w - 18);
    draw_text_clipped(l->quick_files_button.x + 10, l->quick_files_button.y + 9,
                      "FILES", kPalette.text_primary, 1, l->quick_files_button.w - 16);
    draw_text_clipped(l->quick_doom_button.x + 11, l->quick_doom_button.y + 9,
                      "DOOM", kPalette.text_primary, 1, l->quick_doom_button.w - 16);

    for (int slot = 0; slot < kAppWindowCount; ++slot) {
        rect_i btn;
        int app_idx = 0;
        if (!app_task_button_rect(l, slot, &btn, &app_idx)) {
            break;
        }

        const uint32_t fill = s_app_windows[app_idx].minimized ? 0xE3DFD4 : 0xD7E2F2;
        bb_fill_round_rect(btn.x, btn.y, btn.w, btn.h, 6, fill);
        bb_draw_border(btn.x, btn.y, btn.w, btn.h, 0x6D7D92);
        const rect_i icon = rect_make(btn.x + 2, btn.y + 3, 16, 16);
        draw_app_icon_badge(&icon, app_idx, false);
        draw_text_clipped(btn.x + 21, btn.y + 8, kAppWindowTitles[app_idx], kPalette.text_primary, 1, btn.w - 24);
    }

    char uptime[16];
    uptime[0] = '\0';
    format_seconds_hms(s_ticks / kTicksPerSecondEstimate, uptime, sizeof(uptime));
    draw_text_clipped(l->clock_box.x + 6, l->clock_box.y + 9, uptime, kPalette.text_primary, 1, l->clock_box.w - 12);
    if (session_logged_in()) {
        const char* role = (s_session_user == SESSION_USER_GUEST) ? "GUEST" : "ROOT";
        const uint32_t role_color = (s_session_user == SESSION_USER_GUEST) ? 0x9FE2B8 : 0xF4D59A;
        draw_text_clipped(l->clock_box.x + 50, l->clock_box.y + 1, role, role_color, 1, l->clock_box.w - 52);
    }
}

static void draw_start_menu(const ui_layout* l) {
    if (!s_start_menu_open) {
        return;
    }

    bb_draw_soft_panel(l->start_menu.x, l->start_menu.y, l->start_menu.w, l->start_menu.h, 10, s_theme_menu_bg, 0x324A69);
    bb_draw_vgradient(l->start_menu.x + 2, l->start_menu.y + 2, l->start_menu.w - 4, kStartMenuHeaderH - 2, 0x1C4E89, 0x0A246A);
    bb_draw_border(l->start_menu.x + 2, l->start_menu.y + 2, l->start_menu.w - 4, kStartMenuHeaderH - 2, 0x06163F);
    draw_text_clipped(l->start_menu.x + 10, l->start_menu.y + 7, "PYCOREOS APPS", 0xFFFFFF, 1, 116);
    draw_text_clipped(l->start_menu.x + 10, l->start_menu.y + 15, "Search + Quick Access", 0xD9E6F6, 1, 126);

    const rect_i search = start_menu_search_rect(l);
    bb_draw_sunken_box(search.x, search.y, search.w, search.h, 0xFFFFFF);
    if (s_start_search_focused) {
        bb_draw_border(search.x, search.y, search.w, search.h, 0x3465A4);
    }
    if (s_start_search_len == 0) {
        draw_text_clipped(search.x + 4, search.y + 4, "Search apps", 0x7A7A7A, 1, search.w - 8);
    } else {
        draw_text_clipped(search.x + 4, search.y + 4, s_start_search, 0x1A1A1A, 1, search.w - 8);
    }
    if (s_start_search_focused && s_input_cursor_visible) {
        const int char_w = 6 * ui_text_scale(1);
        int cursor_x = search.x + 4 + (int)s_start_search_len * char_w;
        const int cursor_max = search.x + search.w - 4;
        if (cursor_x > cursor_max) {
            cursor_x = cursor_max;
        }
        bb_fill_rect(cursor_x, search.y + 3, 1, search.h - 6, 0x1A1A1A);
    }

    static const char* kQuickLabels[3] = {"HELP", "FILES", "SET"};
    for (int i = 0; i < 3; ++i) {
        const rect_i q = start_menu_quick_rect(l, i);
        const bool hover = rect_contains(q, s_mouse_x, s_mouse_y);
        bb_fill_round_rect(q.x, q.y, q.w, q.h, 4, hover ? 0x244062 : 0x1D2D43);
        bb_draw_border(q.x, q.y, q.w, q.h, 0x2F4A6A);
        draw_text_clipped(q.x + 5, q.y + 4, kQuickLabels[i], 0xE8EDF5, 1, q.w - 8);
    }

    static const char* kPowerLabels[4] = {"SLP", "OUT", "RST", "OFF"};
    for (int i = 0; i < 4; ++i) {
        const rect_i pwr = start_menu_power_rect(l, i);
        const bool hover = rect_contains(pwr, s_mouse_x, s_mouse_y);
        const uint32_t fill = hover ? color_lerp(0xA14545, 0xC06D4C, hover_anim_t(i * 9)) : 0x9A3E3E;
        bb_fill_round_rect(pwr.x, pwr.y, pwr.w, pwr.h, 4, fill);
        bb_draw_border(pwr.x, pwr.y, pwr.w, pwr.h, 0x4D1515);
        draw_text_clipped(pwr.x + 4, pwr.y + 4, kPowerLabels[i], 0xFFFFFF, 1, pwr.w - 6);
    }

    int row = 0;
    for (int i = 0; i < kStartMenuItems; ++i) {
        if (!start_menu_item_matches(i)) {
            continue;
        }
        const rect_i item = start_menu_item_rect(l, row);
        ++row;

        const bool hover = rect_contains(item, s_mouse_x, s_mouse_y);
        const rect_i icon = rect_make(item.x + 6, item.y + 1, 10, 10);
        const int text_x = icon.x + icon.w + 4;
        const int text_w = item.w - (text_x - item.x) - 6;
        if (hover) {
            bb_fill_round_rect(item.x, item.y, item.w, item.h, 4,
                               color_lerp(s_theme_menu_hover_bg, 0x164A98, hover_anim_t(i * 5)));
            draw_start_menu_icon(&icon, i);
            draw_text_clipped(text_x, item.y + 3, kStartMenuLabels[i], s_theme_menu_hover_text, 1, text_w);
        } else {
            draw_start_menu_icon(&icon, i);
            draw_text_clipped(text_x, item.y + 3, kStartMenuLabels[i], kPalette.text_primary, 1, text_w);
        }
    }

    if (row == 0) {
        draw_text_clipped(l->start_menu.x + 10, l->start_menu.y + kStartMenuHeaderH + 10,
                          "No matching apps", 0x555555, 1, l->start_menu.w - 20);
    }
}

static const char* tooltip_for_pointer(const ui_layout* l) {
    if (rect_contains(l->start_button, s_mouse_x, s_mouse_y)) {
        return "Open Start menu";
    }
    if (rect_contains(l->task_terminal_button, s_mouse_x, s_mouse_y)) {
        return "Toggle terminal";
    }
    if (rect_contains(l->quick_help_button, s_mouse_x, s_mouse_y)) {
        return "Quick launch: Help";
    }
    if (rect_contains(l->quick_files_button, s_mouse_x, s_mouse_y)) {
        return "Quick launch: Files";
    }
    if (rect_contains(l->quick_doom_button, s_mouse_x, s_mouse_y)) {
        return "Quick launch: Doom";
    }

    for (int slot = 0; slot < kAppWindowCount; ++slot) {
        rect_i btn;
        int app_idx = 0;
        if (!app_task_button_rect(l, slot, &btn, &app_idx)) {
            break;
        }
        if (rect_contains(btn, s_mouse_x, s_mouse_y)) {
            return s_app_windows[app_idx].minimized ? "Restore app from taskbar" : "Minimize app to taskbar";
        }
    }

    if (s_start_menu_open) {
        if (rect_contains(start_menu_search_rect(l), s_mouse_x, s_mouse_y)) {
            return "Type to filter app list";
        }
        for (int i = 0; i < 3; ++i) {
            if (rect_contains(start_menu_quick_rect(l, i), s_mouse_x, s_mouse_y)) {
                return "Quick-access app";
            }
        }
        static const char* kPowerTips[4] = {
            "Sleep mode", "Log out session", "Restart system", "Shut down system"
        };
        for (int i = 0; i < 4; ++i) {
            if (rect_contains(start_menu_power_rect(l, i), s_mouse_x, s_mouse_y)) {
                return kPowerTips[i];
            }
        }
        const int item = start_menu_item_index_at(l, s_mouse_x, s_mouse_y);
        if (item >= 0) {
            return kStartMenuLabels[item];
        }
    }

    for (int i = 0; i < kDesktopIconCount; ++i) {
        if (rect_contains(desktop_icon_hit_rect(l, i), s_mouse_x, s_mouse_y)) {
            return kDesktopIconLabels[i];
        }
    }

    const int app_idx = topmost_app_window_at_point(s_mouse_x, s_mouse_y);
    if (app_idx >= 0) {
        if (rect_contains(app_window_close_rect(app_idx), s_mouse_x, s_mouse_y)) {
            return "Close window";
        }
        if (rect_contains(app_window_max_rect(app_idx), s_mouse_x, s_mouse_y)) {
            return s_app_windows[app_idx].maximized ? "Restore window" : "Maximize window";
        }
        if (rect_contains(app_window_min_rect(app_idx), s_mouse_x, s_mouse_y)) {
            return "Minimize to taskbar";
        }
        if (rect_contains(app_window_resize_rect(app_idx), s_mouse_x, s_mouse_y)) {
            return "Resize app";
        }
        if (rect_contains(app_window_title_rect(app_idx), s_mouse_x, s_mouse_y)) {
            return "Drag app window";
        }
    }

    return NULL;
}

static void draw_tooltip(const ui_layout* l) {
    const char* tip = tooltip_for_pointer(l);

    if (tip == NULL || tip[0] == '\0') {
        if (s_last_tooltip_visible) {
            request_redraw_rect(s_last_tooltip_rect.x, s_last_tooltip_rect.y,
                                s_last_tooltip_rect.w, s_last_tooltip_rect.h);
            s_last_tooltip_visible = false;
        }
        return;
    }

    const int w = (int)cstr_len(tip) * 6 + 12;
    const int h = 18;
    int x = s_mouse_x + 14;
    int y = s_mouse_y + 12;
    if (x + w > l->screen_w - 4) {
        x = l->screen_w - w - 4;
    }
    if (y + h > l->taskbar.y - 4) {
        y = l->taskbar.y - h - 4;
    }
    if (x < 2) x = 2;
    if (y < 2) y = 2;

    const rect_i new_tip = rect_make(x, y, w + 2, h + 2);
    if (s_last_tooltip_visible &&
        (s_last_tooltip_rect.x != new_tip.x ||
         s_last_tooltip_rect.y != new_tip.y ||
         s_last_tooltip_rect.w != new_tip.w ||
         s_last_tooltip_rect.h != new_tip.h)) {
        request_redraw_rect(s_last_tooltip_rect.x, s_last_tooltip_rect.y,
                            s_last_tooltip_rect.w, s_last_tooltip_rect.h);
    }
    s_last_tooltip_rect = new_tip;
    s_last_tooltip_visible = true;

    bb_fill_round_rect_alpha(x + 2, y + 2, w, h, 4, 0x000000, 80U);
    bb_fill_round_rect(x, y, w, h, 4, 0xFFF9D6);
    bb_draw_border(x, y, w, h, 0x746A3E);
    draw_text_clipped(x + 6, y + 6, tip, 0x1F1F1F, 1, w - 8);
}

static void draw_boot_animation_overlay(const ui_layout* l) {
    if (s_boot_anim_tick >= kBootAnimFrames) {
        return;
    }

    uint32_t alpha = 220U;
    if (s_boot_anim_tick > 72U) {
        const uint32_t remain = (uint32_t)(kBootAnimFrames - s_boot_anim_tick);
        const uint32_t tail = (uint32_t)(kBootAnimFrames - 72U);
        alpha = (remain * 220U) / (tail == 0U ? 1U : tail);
    }

    bb_blend_rect(0, 0, l->screen_w, l->taskbar.y, 0x0A1B33, alpha);
    const rect_i panel = rect_make((l->screen_w - 360) / 2, (l->taskbar.y - 168) / 2, 360, 168);
    bb_draw_soft_panel(panel.x, panel.y, panel.w, panel.h, 14, 0x112A4C, 0x5379A5);
    draw_text_clipped(panel.x + 20, panel.y + 22, "PYCOREOS", 0xFFFFFF, 2, panel.w - 40);
    draw_text_clipped(panel.x + 20, panel.y + 48, "Booting desktop experience...", 0xCDE0F8, 1, panel.w - 40);

    const rect_i bar = rect_make(panel.x + 20, panel.y + panel.h - 34, panel.w - 40, 14);
    bb_draw_sunken_box(bar.x, bar.y, bar.w, bar.h, 0x0B1629);
    const int fill_w = (int)((uint32_t)(bar.w - 4) * clamp_u32(s_boot_anim_tick, 0U, kBootAnimFrames) / kBootAnimFrames);
    bb_fill_round_rect(bar.x + 2, bar.y + 2, fill_w, bar.h - 4, 4, 0x6DA1E3);
}

static void draw_sleep_overlay(const ui_layout* l) {
    if (!s_sleeping) {
        return;
    }
    bb_blend_rect(0, 0, l->screen_w, l->taskbar.y, 0x03050A, 198U);
    draw_text_clipped((l->screen_w / 2) - 84, (l->taskbar.y / 2) - 10,
                      "Sleeping... move mouse or press any key", 0xE6ECF7, 1, 260);
}

static void draw_login_overlay(const ui_layout* l) {
    if (session_logged_in()) {
        return;
    }

    bb_blend_rect(0, 0, l->screen_w, l->screen_h, 0x02060D, 214U);
    const rect_i panel = login_panel_rect();
    bb_draw_soft_panel(panel.x, panel.y, panel.w, panel.h, 14, 0x0F1A27, 0x2D4A6B);
    draw_text_clipped(panel.x + 20, panel.y + 18, "PYCOREOS SIGN IN", 0xEAF2FF, 2, panel.w - 40);
    draw_text_clipped(panel.x + 20, panel.y + 46,
                      "Choose account: root (PIN) or guest (no password)", 0xAFC2D9, 1, panel.w - 40);

    const rect_i root_box = login_root_rect();
    const rect_i guest_box = login_guest_rect();
    const bool guest_selected = s_login_guest_selected;
    const uint32_t root_fill = guest_selected ? 0x1A2736 : 0x234E82;
    const uint32_t guest_fill = guest_selected ? 0x1F5E3E : 0x1A2736;
    const uint32_t root_border = guest_selected ? 0x32475E : 0x7FA6D4;
    const uint32_t guest_border = guest_selected ? 0x8ABDA1 : 0x32475E;

    bb_fill_round_rect(root_box.x, root_box.y, root_box.w, root_box.h, 8, root_fill);
    bb_fill_round_rect(guest_box.x, guest_box.y, guest_box.w, guest_box.h, 8, guest_fill);
    bb_draw_border(root_box.x, root_box.y, root_box.w, root_box.h, root_border);
    bb_draw_border(guest_box.x, guest_box.y, guest_box.w, guest_box.h, guest_border);

    draw_text_clipped(root_box.x + 10, root_box.y + 12, "root", 0xFFFFFF, 2, root_box.w - 20);
    draw_text_clipped(root_box.x + 10, root_box.y + 40, "Admin account (PIN required)", 0xD5E5F8, 1, root_box.w - 16);
    draw_text_clipped(guest_box.x + 10, guest_box.y + 12, "guest", 0xFFFFFF, 2, guest_box.w - 20);
    draw_text_clipped(guest_box.x + 10, guest_box.y + 40, "No password required", 0xD5F0DF, 1, guest_box.w - 16);

    const rect_i pin_box = login_pin_rect();
    bb_draw_sunken_box(pin_box.x, pin_box.y, pin_box.w, pin_box.h, 0x0C1520);
    if (guest_selected) {
        draw_text_clipped(pin_box.x + 10, pin_box.y + 11, "PIN disabled for guest sign-in.", 0x8FA3B8, 1, pin_box.w - 16);
    } else {
        char masked[kLoginPinMax + 1];
        for (size_t i = 0; i < s_login_pin_len && i + 1 < sizeof(masked); ++i) {
            masked[i] = '*';
            masked[i + 1] = '\0';
        }
        if (s_login_pin_len == 0) {
            copy_str(masked, sizeof(masked), "Enter PIN");
            draw_text_clipped(pin_box.x + 10, pin_box.y + 11, masked, 0x7F96AD, 1, pin_box.w - 16);
        } else {
            draw_text_clipped(pin_box.x + 10, pin_box.y + 11, masked, 0xE8EDF5, 1, pin_box.w - 16);
        }
    }

    const rect_i login_btn = login_button_rect();
    bb_fill_round_rect(login_btn.x, login_btn.y, login_btn.w, login_btn.h, 6, 0x2A6FC4);
    bb_draw_border(login_btn.x, login_btn.y, login_btn.w, login_btn.h, 0x8EB4E2);
    draw_text_clipped(login_btn.x + 38, login_btn.y + 9, "SIGN IN", 0xFFFFFF, 1, login_btn.w - 12);

    draw_text_clipped(panel.x + 24, panel.y + panel.h - 40, "Tab/mouse: switch account   Enter: sign in", 0x9BB1C7, 1, panel.w - 180);
    if (s_login_message[0] != '\0') {
        draw_text_clipped(panel.x + 24, panel.y + panel.h - 24, s_login_message, 0xF48C8C, 1, panel.w - 180);
    }
}

static uint8_t wm_resize_edges_at(const ui_layout* l, int x, int y) {
    if (s_terminal_window.minimized || s_terminal_window.maximized) {
        return 0;
    }

    const int wx0 = l->window.x;
    const int wy0 = l->window.y;
    const int wx1 = l->window.x + l->window.w - 1;
    const int wy1 = l->window.y + l->window.h - 1;
    const int e = kResizeEdgeTolerance;

    const bool in_y_range = y >= wy0 - e && y <= wy1 + e;
    const bool in_x_range = x >= wx0 - e && x <= wx1 + e;
    if (!in_y_range || !in_x_range) {
        return 0;
    }

    const bool near_left = x >= wx0 - e && x <= wx0 + e;
    const bool near_right = x >= wx1 - e && x <= wx1 + e;
    const bool near_top = y >= wy0 - e && y <= wy0 + e;
    const bool near_bottom = y >= wy1 - e && y <= wy1 + e;

    uint8_t edges = 0;
    if (near_left) edges |= kResizeLeft;
    if (near_right) edges |= kResizeRight;
    if (near_top) edges |= kResizeTop;
    if (near_bottom) edges |= kResizeBottom;
    return edges;
}

static void wm_begin_resize(uint8_t edges) {
    s_terminal_window.dragging = false;
    s_terminal_window.resizing = true;
    s_terminal_window.resize_edges = edges;
    s_terminal_window.resize_anchor_mouse_x = s_mouse_x;
    s_terminal_window.resize_anchor_mouse_y = s_mouse_y;
    s_terminal_window.resize_anchor_x = s_terminal_window.x;
    s_terminal_window.resize_anchor_y = s_terminal_window.y;
    s_terminal_window.resize_anchor_w = s_terminal_window.w;
    s_terminal_window.resize_anchor_h = s_terminal_window.h;
}

static void wm_apply_resize(const ui_layout* l) {
    if (!s_terminal_window.resizing || s_terminal_window.resize_edges == 0) {
        return;
    }

    int left = s_terminal_window.resize_anchor_x;
    int top = s_terminal_window.resize_anchor_y;
    int right = left + s_terminal_window.resize_anchor_w;
    int bottom = top + s_terminal_window.resize_anchor_h;

    const int dx = s_mouse_x - s_terminal_window.resize_anchor_mouse_x;
    const int dy = s_mouse_y - s_terminal_window.resize_anchor_mouse_y;
    const uint8_t edges = s_terminal_window.resize_edges;

    if ((edges & kResizeLeft) != 0) {
        left += dx;
    }
    if ((edges & kResizeRight) != 0) {
        right += dx;
    }
    if ((edges & kResizeTop) != 0) {
        top += dy;
    }
    if ((edges & kResizeBottom) != 0) {
        bottom += dy;
    }

    if (right - left < kWindowMinW) {
        if ((edges & kResizeLeft) != 0 && (edges & kResizeRight) == 0) {
            left = right - kWindowMinW;
        } else {
            right = left + kWindowMinW;
        }
    }
    if (bottom - top < kWindowMinH) {
        if ((edges & kResizeTop) != 0 && (edges & kResizeBottom) == 0) {
            top = bottom - kWindowMinH;
        } else {
            bottom = top + kWindowMinH;
        }
    }

    const int min_x = 2;
    const int min_y = 2;
    const int max_x = l->screen_w - 2;
    const int max_y = l->taskbar.y - 2;

    if (left < min_x) {
        if ((edges & kResizeLeft) != 0 && (edges & kResizeRight) == 0) {
            left = min_x;
        } else {
            const int shift = min_x - left;
            left += shift;
            right += shift;
        }
    }
    if (top < min_y) {
        if ((edges & kResizeTop) != 0 && (edges & kResizeBottom) == 0) {
            top = min_y;
        } else {
            const int shift = min_y - top;
            top += shift;
            bottom += shift;
        }
    }
    if (right > max_x) {
        if ((edges & kResizeRight) != 0 && (edges & kResizeLeft) == 0) {
            right = max_x;
        } else {
            const int shift = right - max_x;
            left -= shift;
            right -= shift;
        }
    }
    if (bottom > max_y) {
        if ((edges & kResizeBottom) != 0 && (edges & kResizeTop) == 0) {
            bottom = max_y;
        } else {
            const int shift = bottom - max_y;
            top -= shift;
            bottom -= shift;
        }
    }

    if (right - left < kWindowMinW) {
        right = left + kWindowMinW;
    }
    if (bottom - top < kWindowMinH) {
        bottom = top + kWindowMinH;
    }
    if (right > max_x) {
        const int shift = right - max_x;
        right -= shift;
        left -= shift;
    }
    if (bottom > max_y) {
        const int shift = bottom - max_y;
        bottom -= shift;
        top -= shift;
    }

    if (left < min_x) {
        left = min_x;
    }
    if (top < min_y) {
        top = min_y;
    }

    const int next_w = right - left;
    const int next_h = bottom - top;
    if (left != s_terminal_window.x || top != s_terminal_window.y ||
        next_w != s_terminal_window.w || next_h != s_terminal_window.h) {
        s_terminal_window.x = left;
        s_terminal_window.y = top;
        s_terminal_window.w = next_w;
        s_terminal_window.h = next_h;
        request_redraw();
    }
}

static cursor_context derive_cursor_context(const ui_layout* l) {
    if (!session_logged_in()) {
        if (rect_contains(login_root_rect(), s_mouse_x, s_mouse_y) ||
            rect_contains(login_guest_rect(), s_mouse_x, s_mouse_y) ||
            rect_contains(login_button_rect(), s_mouse_x, s_mouse_y)) {
            return CURSOR_CONTEXT_CLICKABLE;
        }
        if (!s_login_guest_selected && rect_contains(login_pin_rect(), s_mouse_x, s_mouse_y)) {
            return CURSOR_CONTEXT_TEXT;
        }
        return CURSOR_CONTEXT_DEFAULT;
    }

    if (rect_contains(l->start_button, s_mouse_x, s_mouse_y) ||
        rect_contains(l->task_terminal_button, s_mouse_x, s_mouse_y) ||
        rect_contains(l->quick_help_button, s_mouse_x, s_mouse_y) ||
        rect_contains(l->quick_files_button, s_mouse_x, s_mouse_y) ||
        rect_contains(l->quick_doom_button, s_mouse_x, s_mouse_y)) {
        return CURSOR_CONTEXT_CLICKABLE;
    }

    for (int slot = 0; slot < kAppWindowCount; ++slot) {
        rect_i btn;
        if (!app_task_button_rect(l, slot, &btn, NULL)) {
            break;
        }
        if (rect_contains(btn, s_mouse_x, s_mouse_y)) {
            return CURSOR_CONTEXT_CLICKABLE;
        }
    }

    if (s_start_menu_open) {
        if (rect_contains(l->start_menu, s_mouse_x, s_mouse_y)) {
            if (rect_contains(start_menu_search_rect(l), s_mouse_x, s_mouse_y)) {
                return CURSOR_CONTEXT_TEXT;
            }
            for (int i = 0; i < 3; ++i) {
                if (rect_contains(start_menu_quick_rect(l, i), s_mouse_x, s_mouse_y)) {
                    return CURSOR_CONTEXT_CLICKABLE;
                }
            }
            for (int i = 0; i < 4; ++i) {
                if (rect_contains(start_menu_power_rect(l, i), s_mouse_x, s_mouse_y)) {
                    return CURSOR_CONTEXT_CLICKABLE;
                }
            }
            if (start_menu_item_index_at(l, s_mouse_x, s_mouse_y) >= 0) {
                return CURSOR_CONTEXT_CLICKABLE;
            }
        }
    }

    const int app_idx = topmost_app_window_at_point(s_mouse_x, s_mouse_y);
    if (app_idx >= 0) {
        if (rect_contains(app_window_close_rect(app_idx), s_mouse_x, s_mouse_y) ||
            rect_contains(app_window_max_rect(app_idx), s_mouse_x, s_mouse_y) ||
            rect_contains(app_window_min_rect(app_idx), s_mouse_x, s_mouse_y) ||
            rect_contains(app_window_title_rect(app_idx), s_mouse_x, s_mouse_y)) {
            return CURSOR_CONTEXT_CLICKABLE;
        }
        if (!s_app_windows[app_idx].maximized &&
            rect_contains(app_window_resize_rect(app_idx), s_mouse_x, s_mouse_y)) {
            return CURSOR_CONTEXT_RESIZE_NWSE;
        }

        const rect_i content = app_window_content_rect(app_idx);
        if (app_idx == APP_NOTES && rect_contains(notes_text_rect(&content), s_mouse_x, s_mouse_y)) {
            return CURSOR_CONTEXT_TEXT;
        }
        if (app_idx == APP_EDITOR && rect_contains(editor_text_rect(&content), s_mouse_x, s_mouse_y)) {
            return CURSOR_CONTEXT_TEXT;
        }
        if (app_idx == APP_SETTINGS &&
            (rect_contains(settings_mouse_minus_rect(&content), s_mouse_x, s_mouse_y) ||
             rect_contains(settings_mouse_plus_rect(&content), s_mouse_x, s_mouse_y) ||
             rect_contains(settings_theme_prev_rect(&content), s_mouse_x, s_mouse_y) ||
             rect_contains(settings_theme_next_rect(&content), s_mouse_x, s_mouse_y) ||
             rect_contains(settings_resolution_toggle_rect(&content), s_mouse_x, s_mouse_y) ||
             rect_contains(settings_save_rect(&content), s_mouse_x, s_mouse_y))) {
            return CURSOR_CONTEXT_CLICKABLE;
        }
        if (app_idx == APP_FILES) {
            for (int row = 0; row < kFileRowsVisible; ++row) {
                if (rect_contains(files_row_rect(&content, row), s_mouse_x, s_mouse_y)) {
                    return CURSOR_CONTEXT_CLICKABLE;
                }
            }
        }
        if (app_idx == APP_NOTES && rect_contains(notes_save_rect(&content), s_mouse_x, s_mouse_y)) {
            return CURSOR_CONTEXT_CLICKABLE;
        }
        if (app_idx == APP_EDITOR && rect_contains(editor_save_rect(&content), s_mouse_x, s_mouse_y)) {
            return CURSOR_CONTEXT_CLICKABLE;
        }
        return CURSOR_CONTEXT_DEFAULT;
    }

    if (!s_terminal_window.minimized) {
        const uint8_t edges = wm_resize_edges_at(l, s_mouse_x, s_mouse_y);
        if ((edges & (kResizeLeft | kResizeTop)) == (kResizeLeft | kResizeTop) ||
            (edges & (kResizeRight | kResizeBottom)) == (kResizeRight | kResizeBottom)) {
            return CURSOR_CONTEXT_RESIZE_NWSE;
        }
        if ((edges & (kResizeRight | kResizeTop)) == (kResizeRight | kResizeTop) ||
            (edges & (kResizeLeft | kResizeBottom)) == (kResizeLeft | kResizeBottom)) {
            return CURSOR_CONTEXT_RESIZE_NESW;
        }
        if ((edges & (kResizeLeft | kResizeRight)) != 0) {
            return CURSOR_CONTEXT_RESIZE_EW;
        }
        if ((edges & (kResizeTop | kResizeBottom)) != 0) {
            return CURSOR_CONTEXT_RESIZE_NS;
        }

        if (rect_contains(l->btn_min, s_mouse_x, s_mouse_y) ||
            rect_contains(l->btn_max, s_mouse_x, s_mouse_y) ||
            rect_contains(l->btn_close, s_mouse_x, s_mouse_y) ||
            rect_contains(l->titlebar, s_mouse_x, s_mouse_y)) {
            return CURSOR_CONTEXT_CLICKABLE;
        }

        if (rect_contains(l->input_box, s_mouse_x, s_mouse_y)) {
            return CURSOR_CONTEXT_TEXT;
        }
    }

    if (s_terminal_window.minimized ||
        !rect_contains(l->window, s_mouse_x, s_mouse_y)) {
        for (int i = 0; i < kDesktopIconCount; ++i) {
            if (rect_contains(desktop_icon_hit_rect(l, i), s_mouse_x, s_mouse_y)) {
                return CURSOR_CONTEXT_CLICKABLE;
            }
        }
    }

    return CURSOR_CONTEXT_DEFAULT;
}

static void update_cursor_target(void) {
    if (!s_graphics) {
        return;
    }

    ui_layout l;
    compute_layout(&l);
    s_cursor_context = derive_cursor_context(&l);
    cursor_manager_set_position(s_mouse_x, s_mouse_y);
    cursor_manager_set_context(s_cursor_context);
}

static void build_static_cache(const ui_layout* l) {
    s_draw_target = s_static_cache;
    s_clip_enabled = false;

    draw_background(l);
    draw_desktop_icons(l);
    draw_terminal_window_chrome(l);
    draw_taskbar_chrome(l);

    s_static_cache_valid = true;
    s_draw_target = s_backbuffer;
}

static void draw_desktop_graphics(rect_i dirty) {
    ui_layout l;
    compute_layout(&l);

    if (!session_logged_in()) {
        s_draw_target = s_backbuffer;
        s_clip_enabled = false;
        bb_draw_vgradient(0, 0, l.screen_w, l.screen_h, 0x0A1626, 0x04080F);
        for (int y = 0; y < l.screen_h; y += 32) {
            bb_blend_rect(0, y, l.screen_w, 1, 0xFFFFFF, 12U);
        }
        draw_login_overlay(&l);
        framebuffer_present_argb8888_rect(s_backbuffer, kBackbufferMaxW, dirty.x, dirty.y, dirty.w, dirty.h);
        return;
    }

    if (!s_static_cache_valid) {
        build_static_cache(&l);
    }

    bb_copy_rect(s_backbuffer, s_static_cache, dirty);

    const rect_i prev_clip = s_clip_rect;
    const bool prev_clip_enabled = s_clip_enabled;
    s_clip_rect = dirty;
    s_clip_enabled = true;

    draw_terminal_window_dynamic(&l);
    draw_app_windows(&l);
    draw_taskbar_dynamic(&l);
    draw_start_menu(&l);
    draw_boot_animation_overlay(&l);
    draw_sleep_overlay(&l);
    if (!session_logged_in()) {
        draw_login_overlay(&l);
    } else {
        draw_tooltip(&l);
    }

    s_clip_rect = prev_clip;
    s_clip_enabled = prev_clip_enabled;

    framebuffer_present_argb8888_rect(s_backbuffer, kBackbufferMaxW, dirty.x, dirty.y, dirty.w, dirty.h);
}

static void draw_text_mode_fallback(void) {
    const uint8_t sky = 0x1F;
    const uint8_t panel = 0x17;

    console_clear(sky);
    if (!session_logged_in()) {
        console_write(" PyCoreOS Login\n", panel);
        console_write(" ----------------------------------------\n", panel);
        if (s_login_guest_selected) {
            console_write("  account: guest\n", panel);
            console_write("  password: none\n", panel);
        } else {
            console_write("  account: root\n", panel);
            console_write("  pin: ", panel);
            for (size_t i = 0; i < s_login_pin_len; ++i) {
                console_write("*", panel);
            }
            console_write("\n", panel);
        }
        if (s_login_message[0] != '\0') {
            console_write("  ", panel);
            console_write(s_login_message, panel);
            console_write("\n", panel);
        }
        console_write("\n  Use: Tab or mouse to switch account, Enter to sign in\n", panel);
        return;
    }

    console_write(" PyCoreOS Desktop\n", panel);
    console_write(" ----------------------------------------\n", panel);
    for (size_t i = 0; i < s_log_count; ++i) {
        console_write("  ", panel);
        console_write(s_log[i], panel);
        console_write("\n", panel);
    }
    console_write("\n  Input> ", panel);
    console_write(s_input_line, 0x1F);
}

static void redraw(void) {
    if (!s_dirty_valid) {
        return;
    }
    rect_i dirty = s_dirty_rect;
    if (s_graphics) {
        int cx = 0;
        int cy = 0;
        int cw = 0;
        int ch = 0;
        if (cursor_manager_get_drawn_bounds(&cx, &cy, &cw, &ch)) {
            dirty = rect_union(dirty, rect_make(cx, cy, cw, ch));
        }
        if (cursor_manager_get_target_bounds(&cx, &cy, &cw, &ch)) {
            dirty = rect_union(dirty, rect_make(cx, cy, cw, ch));
        }
    }
    s_dirty_valid = false;

    if (s_graphics) {
        draw_desktop_graphics(dirty);
        update_cursor_target();
        cursor_manager_set_scene(s_backbuffer, kBackbufferMaxW);
        cursor_manager_on_scene_redraw();
    } else {
        draw_text_mode_fallback();
    }
}

static void execute_start_menu_item(int item_idx) {
    if (item_idx < 0 || item_idx >= kStartMenuItems) {
        return;
    }

    const int action = kStartMenuActions[item_idx];
    if (action < 0) {
        desktop_clear_log();
    } else if (action < kAppWindowCount) {
        const app_id app = (app_id)action;
        open_app_window(app);
        if (app == APP_DOOM) {
            queue_command("doom");
        }
    }

    start_menu_reset_search();
    s_start_menu_open = false;
    request_redraw();
}

static void execute_start_quick_action(int slot) {
    if (slot == 0) {
        open_app_window(APP_HELP);
    } else if (slot == 1) {
        open_app_window(APP_FILES);
    } else if (slot == 2) {
        open_app_window(APP_SETTINGS);
    }
    s_start_menu_open = false;
    start_menu_reset_search();
    request_redraw();
}

static void execute_start_power_action(int slot) {
    if (slot == 0) {
        queue_command("sleep");
    } else if (slot == 1) {
        queue_command("logout");
    } else if (slot == 2) {
        queue_command("restart");
    } else if (slot == 3) {
        queue_command("shutdown");
    }
    s_start_menu_open = false;
    start_menu_reset_search();
    request_redraw();
}

static void execute_desktop_icon(int icon_idx) {
    if (icon_idx < 0 || icon_idx >= kDesktopIconCount) {
        return;
    }

    const app_id app = kDesktopIconApps[icon_idx];
    open_app_window(app);
    if (app == APP_DOOM) {
        queue_command("doom");
    }

    s_start_menu_open = false;
    request_redraw();
}

bool desktop_open_app_by_name(const char* name) {
    app_id app = APP_HELP;
    if (!app_id_from_name(name, &app)) {
        return false;
    }

    open_app_window(app);
    if (app == APP_DOOM) {
        queue_command("doom");
    }
    return true;
}

uint32_t desktop_uptime_seconds(void) {
    return s_ticks / kTicksPerSecondEstimate;
}

void desktop_report_idle_spins(uint32_t idle_spins) {
    s_last_idle_spins = idle_spins;
    if (idle_spins > s_max_idle_spins) {
        s_max_idle_spins = idle_spins;
    }
}

void desktop_enter_sleep_mode(void) {
    s_sleeping = true;
    request_redraw();
}

void desktop_logout_session(void) {
    for (int i = 0; i < kAppWindowCount; ++i) {
        close_app_window((app_id)i);
    }
    app_windows_init();
    s_drag_app_idx = -1;
    s_resize_app_idx = -1;
    s_icon_press_idx = -1;
    s_icon_drag_idx = -1;
    s_icon_drag_moved = false;
    s_start_menu_open = false;
    start_menu_reset_search();
    s_sleeping = false;
    s_session_user = SESSION_USER_NONE;
    login_reset_state();
    s_input_len = 0;
    s_input_line[0] = '\0';
    s_pending_command[0] = '\0';
    s_has_pending_command = false;
    desktop_clear_log();
    request_redraw();
}

uint8_t desktop_theme_index(void) {
    return (uint8_t)s_theme_index;
}

bool desktop_set_theme_index(uint8_t theme_idx) {
    if (theme_idx >= (uint8_t)kThemeCount) {
        return false;
    }
    apply_theme((int)theme_idx);
    settings_save();
    request_redraw();
    return true;
}

uint8_t desktop_resolution_mode(void) {
    return (uint8_t)s_settings_resolution_mode;
}

void desktop_toggle_resolution_mode(void) {
    s_settings_resolution_mode = (s_settings_resolution_mode == 0) ? 1 : 0;
    s_font_profile_16_10_1680x1050 = (s_settings_resolution_mode != 0);
    settings_save();
    request_redraw();
}

uint8_t desktop_mouse_speed(void) {
    return s_setting_mouse_speed;
}

bool desktop_set_mouse_speed(uint8_t speed) {
    if (speed < 1U || speed > 4U) {
        return false;
    }
    s_setting_mouse_speed = speed;
    mouse_set_sensitivity(speed);
    settings_save();
    request_redraw();
    return true;
}

const char* desktop_current_user(void) {
    return session_user_name();
}

static bool handle_files_click(const rect_i* content) {
    for (int row = 0; row < kFileRowsVisible; ++row) {
        const rect_i rr = files_row_rect(content, row);
        if (!rect_contains(rr, s_mouse_x, s_mouse_y)) {
            continue;
        }

        const int index = row;
        char filename[56];
        if (!file_entry_at(index, filename, sizeof(filename), NULL, NULL)) {
            return false;
        }

        s_files_selected = index;
        if (is_text_file_name(filename)) {
            editor_open_file(filename);
            s_notes_focused = false;
            return true;
        }

        char preview[200];
        if (fs_read(filename, preview, sizeof(preview))) {
            desktop_append_log("Binary/non-text file preview:");
            desktop_append_log(preview);
        } else {
            desktop_append_log("Unable to open selected file.");
        }
        return true;
    }
    return false;
}

static bool handle_settings_click(const rect_i* content) {
    if (rect_contains(settings_mouse_minus_rect(content), s_mouse_x, s_mouse_y)) {
        if (s_setting_mouse_speed > 1U) {
            --s_setting_mouse_speed;
            mouse_set_sensitivity(s_setting_mouse_speed);
            settings_save();
            request_redraw();
        }
        return true;
    }
    if (rect_contains(settings_mouse_plus_rect(content), s_mouse_x, s_mouse_y)) {
        if (s_setting_mouse_speed < 4U) {
            ++s_setting_mouse_speed;
            mouse_set_sensitivity(s_setting_mouse_speed);
            settings_save();
            request_redraw();
        }
        return true;
    }
    if (rect_contains(settings_resolution_toggle_rect(content), s_mouse_x, s_mouse_y)) {
        s_settings_resolution_mode = (s_settings_resolution_mode == 0) ? 1 : 0;
        s_font_profile_16_10_1680x1050 = (s_settings_resolution_mode != 0);
        settings_save();
        request_redraw();
        return true;
    }
    if (rect_contains(settings_save_rect(content), s_mouse_x, s_mouse_y)) {
        settings_save();
        desktop_append_log("settings saved");
        request_redraw();
        return true;
    }
    return false;
}

static bool handle_notes_click(const rect_i* content) {
    if (rect_contains(notes_save_rect(content), s_mouse_x, s_mouse_y)) {
        notes_save();
        desktop_append_log("notes saved");
        request_redraw();
        return true;
    }
    if (rect_contains(notes_text_rect(content), s_mouse_x, s_mouse_y)) {
        s_notes_focused = true;
        s_editor_focused = false;
        request_redraw();
        return true;
    }
    s_notes_focused = false;
    return false;
}

static bool handle_editor_click(const rect_i* content) {
    if (rect_contains(editor_save_rect(content), s_mouse_x, s_mouse_y)) {
        editor_save();
        desktop_append_log("editor saved");
        request_redraw();
        return true;
    }
    if (rect_contains(editor_text_rect(content), s_mouse_x, s_mouse_y)) {
        s_editor_focused = true;
        s_notes_focused = false;
        request_redraw();
        return true;
    }
    s_editor_focused = false;
    return false;
}

static bool handle_app_content_click(int app_idx) {
    const rect_i content = app_window_content_rect(app_idx);

    if (app_idx == APP_FILES) {
        return handle_files_click(&content);
    }
    if (app_idx == APP_SETTINGS) {
        return handle_settings_click(&content);
    }
    if (app_idx == APP_NOTES) {
        return handle_notes_click(&content);
    }
    if (app_idx == APP_EDITOR) {
        return handle_editor_click(&content);
    }
    if (app_idx == APP_DOOM && rect_contains(content, s_mouse_x, s_mouse_y)) {
        queue_command("doom");
        return true;
    }
    if (app_idx == APP_CALCULATOR) {
        return handle_calculator_click(&content);
    }
    return false;
}

static void process_pointer_events(bool prev_left_down, bool left_down) {
    ui_layout l;
    compute_layout(&l);

    if (!session_logged_in()) {
        if (!prev_left_down && left_down) {
            login_handle_pointer_click();
        }
        return;
    }

    if (!prev_left_down && left_down) {
        if (rect_contains(l.start_button, s_mouse_x, s_mouse_y)) {
            s_start_menu_open = !s_start_menu_open;
            if (s_start_menu_open) {
                start_menu_reset_search();
                s_start_search_focused = true;
            } else {
                s_start_search_focused = false;
            }
            request_redraw();
            return;
        }

        if (s_start_menu_open) {
            if (rect_contains(l.start_menu, s_mouse_x, s_mouse_y)) {
                for (int i = 0; i < 4; ++i) {
                    if (rect_contains(start_menu_power_rect(&l, i), s_mouse_x, s_mouse_y)) {
                        execute_start_power_action(i);
                        return;
                    }
                }
                for (int i = 0; i < 3; ++i) {
                    if (rect_contains(start_menu_quick_rect(&l, i), s_mouse_x, s_mouse_y)) {
                        execute_start_quick_action(i);
                        return;
                    }
                }
                if (rect_contains(start_menu_search_rect(&l), s_mouse_x, s_mouse_y)) {
                    s_start_search_focused = true;
                    request_redraw();
                    return;
                }
                s_start_search_focused = false;
                const int menu_item = start_menu_item_index_at(&l, s_mouse_x, s_mouse_y);
                if (menu_item >= 0) {
                    execute_start_menu_item(menu_item);
                    return;
                }
            } else {
                s_start_menu_open = false;
                start_menu_reset_search();
                request_redraw();
            }
        }

        for (int slot = 0; slot < kAppWindowCount; ++slot) {
            rect_i btn;
            int app_idx = 0;
            if (!app_task_button_rect(&l, slot, &btn, &app_idx)) {
                break;
            }
            if (!rect_contains(btn, s_mouse_x, s_mouse_y)) {
                continue;
            }
            if (s_app_windows[app_idx].minimized) {
                set_app_minimized((app_id)app_idx, false);
            } else {
                set_app_minimized((app_id)app_idx, true);
            }
            s_notes_focused = false;
            s_editor_focused = false;
            return;
        }

        const int top_app_idx = topmost_app_window_at_point(s_mouse_x, s_mouse_y);
        if (top_app_idx >= 0) {
            app_bring_to_front(top_app_idx);
            s_active_is_terminal = false;
            s_active_app_idx = top_app_idx;
            s_notes_focused = false;
            s_editor_focused = false;

            if (rect_contains(app_window_close_rect(top_app_idx), s_mouse_x, s_mouse_y)) {
                s_app_btn_pressed_idx = top_app_idx;
                s_app_btn_pressed_btn = 2;
                request_redraw();
                return;
            }
            if (rect_contains(app_window_max_rect(top_app_idx), s_mouse_x, s_mouse_y)) {
                s_app_btn_pressed_idx = top_app_idx;
                s_app_btn_pressed_btn = 1;
                request_redraw();
                return;
            }
            if (rect_contains(app_window_min_rect(top_app_idx), s_mouse_x, s_mouse_y)) {
                s_app_btn_pressed_idx = top_app_idx;
                s_app_btn_pressed_btn = 0;
                request_redraw();
                return;
            }
            if (!s_app_windows[top_app_idx].maximized &&
                rect_contains(app_window_resize_rect(top_app_idx), s_mouse_x, s_mouse_y)) {
                s_resize_app_idx = top_app_idx;
                s_resize_app_anchor_w = s_app_windows[top_app_idx].w;
                s_resize_app_anchor_h = s_app_windows[top_app_idx].h;
                s_resize_app_anchor_mouse_x = s_mouse_x;
                s_resize_app_anchor_mouse_y = s_mouse_y;
                request_redraw();
                return;
            }
            if (rect_contains(app_window_title_rect(top_app_idx), s_mouse_x, s_mouse_y) &&
                !s_app_windows[top_app_idx].maximized) {
                s_drag_app_idx = top_app_idx;
                s_drag_app_dx = s_mouse_x - s_app_windows[top_app_idx].x;
                s_drag_app_dy = s_mouse_y - s_app_windows[top_app_idx].y;
            } else if (handle_app_content_click(top_app_idx)) {
                request_redraw();
                return;
            }

            request_redraw();
            return;
        }

        s_notes_focused = false;
        s_editor_focused = false;

        const bool over_terminal_window = !s_terminal_window.minimized &&
                                          rect_contains(l.window, s_mouse_x, s_mouse_y);
        if (!over_terminal_window) {
            for (int i = 0; i < kDesktopIconCount; ++i) {
                if (rect_contains(desktop_icon_hit_rect(&l, i), s_mouse_x, s_mouse_y)) {
                    s_icon_press_idx = i;
                    s_icon_drag_idx = -1;
                    s_icon_drag_dx = s_mouse_x - s_icon_cells[i].x;
                    s_icon_drag_dy = s_mouse_y - s_icon_cells[i].y;
                    s_icon_press_x = s_mouse_x;
                    s_icon_press_y = s_mouse_y;
                    s_icon_drag_moved = false;
                    return;
                }
            }
        }

        if (rect_contains(l.task_terminal_button, s_mouse_x, s_mouse_y)) {
            s_terminal_window.minimized = !s_terminal_window.minimized;
            s_terminal_window.dragging = false;
            s_terminal_window.resizing = false;
            request_redraw();
            return;
        }
        if (rect_contains(l.quick_help_button, s_mouse_x, s_mouse_y)) {
            open_app_window(APP_HELP);
            return;
        }
        if (rect_contains(l.quick_files_button, s_mouse_x, s_mouse_y)) {
            open_app_window(APP_FILES);
            return;
        }
        if (rect_contains(l.quick_doom_button, s_mouse_x, s_mouse_y)) {
            open_app_window(APP_DOOM);
            queue_command("doom");
            return;
        }

        if (!s_terminal_window.minimized) {
            s_active_is_terminal = true;
            s_active_app_idx = -1;
            if (rect_contains(l.btn_min, s_mouse_x, s_mouse_y)) {
                s_terminal_btn_pressed = 1;
                request_redraw();
                return;
            }
            if (rect_contains(l.btn_max, s_mouse_x, s_mouse_y)) {
                s_terminal_btn_pressed = 2;
                request_redraw();
                return;
            }
            if (rect_contains(l.btn_close, s_mouse_x, s_mouse_y)) {
                s_terminal_btn_pressed = 3;
                request_redraw();
                return;
            }

            const uint8_t resize_edges = wm_resize_edges_at(&l, s_mouse_x, s_mouse_y);
            if (resize_edges != 0) {
                wm_begin_resize(resize_edges);
                return;
            }

            if (rect_contains(l.titlebar, s_mouse_x, s_mouse_y) &&
                !rect_contains(l.btn_min, s_mouse_x, s_mouse_y) &&
                !rect_contains(l.btn_max, s_mouse_x, s_mouse_y) &&
                !rect_contains(l.btn_close, s_mouse_x, s_mouse_y) &&
                !s_terminal_window.maximized) {
                s_terminal_window.dragging = true;
                s_terminal_window.resizing = false;
                s_terminal_window.drag_dx = s_mouse_x - s_terminal_window.x;
                s_terminal_window.drag_dy = s_mouse_y - s_terminal_window.y;
            }
        }
    }

    if (prev_left_down && !left_down) {
        if (s_terminal_btn_pressed != 0) {
            ui_layout tl;
            compute_layout(&tl);
            const bool in_min = rect_contains(tl.btn_min, s_mouse_x, s_mouse_y);
            const bool in_max = rect_contains(tl.btn_max, s_mouse_x, s_mouse_y);
            const bool in_close = rect_contains(tl.btn_close, s_mouse_x, s_mouse_y);
            if (s_terminal_btn_pressed == 1 && in_min) {
                s_terminal_window.minimized = true;
                s_terminal_window.dragging = false;
                s_terminal_window.resizing = false;
            } else if (s_terminal_btn_pressed == 2 && in_max) {
                wm_toggle_maximize();
            } else if (s_terminal_btn_pressed == 3 && in_close) {
                s_terminal_window.minimized = true;
                s_terminal_window.dragging = false;
                s_terminal_window.resizing = false;
                log_push_line("Terminal minimized to taskbar.");
            }
            s_terminal_btn_pressed = 0;
            request_redraw();
        }

        if (s_app_btn_pressed_idx >= 0) {
            const int idx = s_app_btn_pressed_idx;
            const bool in_min = rect_contains(app_window_min_rect(idx), s_mouse_x, s_mouse_y);
            const bool in_max = rect_contains(app_window_max_rect(idx), s_mouse_x, s_mouse_y);
            const bool in_close = rect_contains(app_window_close_rect(idx), s_mouse_x, s_mouse_y);
            if (s_app_btn_pressed_btn == 0 && in_min) {
                wm_dispatch_app_message(idx, WM_MINIMIZE);
            } else if (s_app_btn_pressed_btn == 1 && in_max) {
                wm_dispatch_app_message(idx, s_app_windows[idx].maximized ? WM_RESTORE : WM_MAXIMIZE);
            } else if (s_app_btn_pressed_btn == 2 && in_close) {
                wm_dispatch_app_message(idx, WM_CLOSE);
            }
            s_app_btn_pressed_idx = -1;
        }

        if (s_icon_press_idx >= 0) {
            if (s_icon_drag_moved) {
                snap_icon_to_grid(&l, s_icon_press_idx);
                s_static_cache_valid = false;
                request_redraw();
            } else {
                execute_desktop_icon(s_icon_press_idx);
            }
        }
        s_icon_press_idx = -1;
        s_icon_drag_idx = -1;
        s_icon_drag_moved = false;
        s_drag_app_idx = -1;
        s_resize_app_idx = -1;
        s_terminal_btn_pressed = 0;
        s_terminal_window.dragging = false;
        s_terminal_window.resizing = false;
        s_terminal_window.resize_edges = 0;
    }

    if (left_down && s_icon_press_idx >= 0) {
        if (!s_icon_drag_moved) {
            int dx = s_mouse_x - s_icon_press_x;
            int dy = s_mouse_y - s_icon_press_y;
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;
            if (dx > 3 || dy > 3) {
                s_icon_drag_moved = true;
                s_icon_drag_idx = s_icon_press_idx;
            }
        }

        if (s_icon_drag_idx >= 0) {
            rect_i* cell = &s_icon_cells[s_icon_drag_idx];
            int nx = s_mouse_x - s_icon_drag_dx;
            int ny = s_mouse_y - s_icon_drag_dy;
            nx = clamp_i32(nx, 0, l.screen_w - cell->w);
            ny = clamp_i32(ny, 0, l.taskbar.y - cell->h);
            if (nx != cell->x || ny != cell->y) {
                cell->x = nx;
                cell->y = ny;
                s_static_cache_valid = false;
                request_redraw();
            }
        }
        return;
    }

    if (left_down && s_resize_app_idx >= 0) {
        app_window* w = &s_app_windows[s_resize_app_idx];
        int nw = s_resize_app_anchor_w + (s_mouse_x - s_resize_app_anchor_mouse_x);
        int nh = s_resize_app_anchor_h + (s_mouse_y - s_resize_app_anchor_mouse_y);
        nw = clamp_i32(nw, 220, l.screen_w - w->x - 2);
        nh = clamp_i32(nh, 130, l.taskbar.y - w->y - 2);
        if (nw != w->w || nh != w->h) {
            w->w = nw;
            w->h = nh;
            request_redraw();
        }
        return;
    }

    if (left_down && s_drag_app_idx >= 0) {
        app_window* w = &s_app_windows[s_drag_app_idx];
        int nx = s_mouse_x - s_drag_app_dx;
        int ny = s_mouse_y - s_drag_app_dy;

        int max_x = l.screen_w - w->w - 2;
        int max_y = l.taskbar.y - w->h - 2;
        if (max_x < 2) max_x = 2;
        if (max_y < 2) max_y = 2;

        nx = clamp_i32(nx, 2, max_x);
        ny = clamp_i32(ny, 2, max_y);
        if (nx != w->x || ny != w->y) {
            w->x = nx;
            w->y = ny;
            request_redraw();
        }
        return;
    }

    if (left_down && s_terminal_window.resizing) {
        wm_apply_resize(&l);
        return;
    }

    if (left_down && s_terminal_window.dragging) {
        const int desktop_h = l.taskbar.y;
        int nx = s_mouse_x - s_terminal_window.drag_dx;
        int ny = s_mouse_y - s_terminal_window.drag_dy;
        nx = clamp_i32(nx, 2, l.screen_w - s_terminal_window.w - 2);
        ny = clamp_i32(ny, 2, desktop_h - s_terminal_window.h - 2);

        if (nx != s_terminal_window.x || ny != s_terminal_window.y) {
            s_terminal_window.x = nx;
            s_terminal_window.y = ny;
            request_redraw();
        }
    }
}

static void process_pointer_wheel(int wheel_delta) {
    if (wheel_delta == 0) {
        return;
    }

    if (!session_logged_in()) {
        return;
    }

    int total = s_mouse_wheel_accum + wheel_delta;
    if (total > 32767) {
        total = 32767;
    } else if (total < -32768) {
        total = -32768;
    }
    s_mouse_wheel_accum = total;

    if (s_terminal_window.minimized) {
        request_redraw_rect(0, 0, kScreenWidth, kScreenHeight);
        return;
    }

    ui_layout l;
    compute_layout(&l);
    if (!rect_contains(l.log_box, s_mouse_x, s_mouse_y)) {
        request_redraw_rect(l.status_box.x, l.status_box.y, l.status_box.w, l.status_box.h);
        return;
    }

    const int max_scroll = terminal_log_max_scroll(&l);
    const int next_scroll = clamp_i32(s_log_scroll + wheel_delta, 0, max_scroll);
    if (next_scroll != s_log_scroll) {
        s_log_scroll = next_scroll;
        request_redraw_log_and_status();
        return;
    }

    request_redraw_rect(l.status_box.x, l.status_box.y, l.status_box.w, l.status_box.h);
}

static void apply_mouse_frame_state(void) {
    if (!s_mouse_pending && s_pending_mouse_wheel == 0) {
        return;
    }

    const bool prev_left = s_mouse_left;
    const bool prev_right = s_mouse_right;
    const bool prev_middle = s_mouse_middle;
    const int old_x = s_mouse_x;
    const int old_y = s_mouse_y;

    const bool had_pointer_update = s_mouse_pending;
    const int wheel_delta = s_pending_mouse_wheel;

    if (had_pointer_update) {
        s_mouse_x = s_pending_mouse_x;
        s_mouse_y = s_pending_mouse_y;
        s_mouse_left = s_pending_mouse_left;
        s_mouse_right = s_pending_mouse_right;
        s_mouse_middle = s_pending_mouse_middle;
        s_mouse_pending = false;
    }
    s_pending_mouse_wheel = 0;

    if (s_sleeping) {
        if (had_pointer_update || wheel_delta != 0) {
            s_sleeping = false;
            request_redraw();
        }
        return;
    }

    if (had_pointer_update) {
        process_pointer_events(prev_left, s_mouse_left);
    }
    process_pointer_wheel(wheel_delta);

    if (s_start_menu_open && (old_x != s_mouse_x || old_y != s_mouse_y)) {
        ui_layout l;
        compute_layout(&l);
        request_redraw_rect(l.start_menu.x, l.start_menu.y, l.start_menu.w, l.start_menu.h);
    }

    if (had_pointer_update && (old_x != s_mouse_x || old_y != s_mouse_y)) {
        ui_layout l;
        compute_layout(&l);

        const bool old_help_hover = rect_contains(l.quick_help_button, old_x, old_y);
        const bool new_help_hover = rect_contains(l.quick_help_button, s_mouse_x, s_mouse_y);
        if (old_help_hover != new_help_hover) {
            request_redraw_rect(l.quick_help_button.x, l.quick_help_button.y, l.quick_help_button.w, l.quick_help_button.h);
        }

        const bool old_files_hover = rect_contains(l.quick_files_button, old_x, old_y);
        const bool new_files_hover = rect_contains(l.quick_files_button, s_mouse_x, s_mouse_y);
        if (old_files_hover != new_files_hover) {
            request_redraw_rect(l.quick_files_button.x, l.quick_files_button.y, l.quick_files_button.w, l.quick_files_button.h);
        }

        const bool old_doom_hover = rect_contains(l.quick_doom_button, old_x, old_y);
        const bool new_doom_hover = rect_contains(l.quick_doom_button, s_mouse_x, s_mouse_y);
        if (old_doom_hover != new_doom_hover) {
            request_redraw_rect(l.quick_doom_button.x, l.quick_doom_button.y, l.quick_doom_button.w, l.quick_doom_button.h);
        }
    }

    const bool pointer_visual_changed = (old_x != s_mouse_x) || (old_y != s_mouse_y) ||
                                        (prev_left != s_mouse_left) ||
                                        (prev_right != s_mouse_right) ||
                                        (prev_middle != s_mouse_middle);
    if (!pointer_visual_changed) {
        return;
    }

    update_cursor_target();

    if (!s_graphics) {
        request_redraw();
        return;
    }

    if (!s_needs_redraw) {
        cursor_manager_step();
    }
}

void desktop_append_log(const char* line) {
    if (line == NULL || line[0] == '\0') {
        return;
    }

    size_t i = 0;
    while (line[i] != '\0') {
        if (line[i] == '\n') {
            ++i;
            continue;
        }

        char chunk[kLogLineLen];
        size_t out = 0;
        while (line[i] != '\0' && line[i] != '\n' && out + 1 < sizeof(chunk) && out < kLogWrapChars) {
            chunk[out++] = line[i++];
        }
        chunk[out] = '\0';
        if (out > 0) {
            log_push_line(chunk);
        }
    }

    request_redraw_log_and_status();
}

void desktop_clear_log(void) {
    for (size_t i = 0; i < kLogLines; ++i) {
        s_log[i][0] = '\0';
    }
    s_log_count = 0;
    s_log_scroll = 0;
    request_redraw_log_and_status();
    request_redraw_input();
}

void desktop_force_redraw(void) {
    s_static_cache_valid = false;
    s_dirty_rect = rect_make(0, 0, kScreenWidth, kScreenHeight);
    s_dirty_valid = true;
    redraw();
    s_needs_redraw = false;
    s_last_frame_tick = s_ticks;
}

void desktop_set_mouse(int x, int y, bool left_down, bool right_down, bool middle_down, int8_t wheel_delta) {
    int nx = x;
    int ny = y;
    const int max_x = kScreenWidth - 1;
    const int max_y = kScreenHeight - 1;
    if (max_x >= 0) {
        nx = clamp_i32(nx, 0, max_x);
    }
    if (max_y >= 0) {
        ny = clamp_i32(ny, 0, max_y);
    }

    s_pending_mouse_x = nx;
    s_pending_mouse_y = ny;
    s_pending_mouse_left = left_down;
    s_pending_mouse_right = right_down;
    s_pending_mouse_middle = middle_down;

    int wheel_total = s_pending_mouse_wheel + (int)wheel_delta;
    if (wheel_total > 127) {
        wheel_total = 127;
    } else if (wheel_total < -128) {
        wheel_total = -128;
    }
    s_pending_mouse_wheel = wheel_total;
    s_mouse_pending = true;
}

void desktop_queue_key(char c) {
    (void)key_queue_push(c);
}

void desktop_init(void) {
    s_ticks = 0;
    s_last_frame_tick = 0;
    s_needs_redraw = true;
    s_blink_frame_counter = 0;
    s_input_cursor_visible = true;
    s_log_scroll = 0;
    s_mouse_wheel_accum = 0;
    s_boot_anim_tick = 0;
    s_sleeping = false;
    s_session_user = SESSION_USER_NONE;
    login_reset_state();

    s_input_len = 0;
    s_input_line[0] = '\0';
    s_pending_command[0] = '\0';
    s_has_pending_command = false;
    s_key_queue_head = 0;
    s_key_queue_tail = 0;
    s_pending_kernel_action = CLI_ACTION_NONE;

    s_graphics = framebuffer_ready();
    s_font_profile_16_10_1680x1050 = false;
    s_settings_resolution_mode = 0;
    s_setting_mouse_speed = 2;
    apply_theme(0);
    s_notes_focused = false;
    s_editor_focused = false;
    s_notes_dirty = false;
    s_editor_dirty = false;
    s_files_selected = -1;
    s_last_idle_spins = 0;
    s_max_idle_spins = 1;
    s_perf_hist_len = 0;
    s_perf_hist_head = 0;
    s_editor_filename[0] = '\0';
    s_editor_text[0] = '\0';
    s_editor_len = 0;
    s_editor_cursor = 0;
    calc_reset();

    wm_init_window();
    app_windows_init();
    s_start_menu_open = false;
    start_menu_reset_search();
    s_icons_initialized = false;
    s_icon_press_idx = -1;
    s_icon_drag_idx = -1;
    s_icon_drag_moved = false;

    s_mouse_x = kScreenWidth / 2;
    s_mouse_y = kScreenHeight / 2;
    s_mouse_left = false;
    s_mouse_right = false;
    s_mouse_middle = false;
    s_pending_mouse_x = s_mouse_x;
    s_pending_mouse_y = s_mouse_y;
    s_pending_mouse_left = s_mouse_left;
    s_pending_mouse_right = s_mouse_right;
    s_pending_mouse_middle = s_mouse_middle;
    s_pending_mouse_wheel = 0;
    s_mouse_pending = false;
    s_cursor_context = CURSOR_CONTEXT_DEFAULT;

    s_draw_target = s_backbuffer;
    s_clip_rect = rect_make(0, 0, kScreenWidth, kScreenHeight);
    s_clip_enabled = false;
    s_dirty_rect = rect_make(0, 0, kScreenWidth, kScreenHeight);
    s_dirty_valid = true;
    s_static_cache_valid = false;

    settings_load();
    notes_load();
    wallpaper_load_from_fs();

    if (s_graphics) {
        cursor_manager_init((uint32_t)kScreenWidth, (uint32_t)kScreenHeight);
        update_cursor_target();
    }

    desktop_clear_log();
    desktop_force_redraw();
}

void desktop_tick(void) {
    ++s_ticks;
    s_last_frame_tick = s_ticks;

    if (s_boot_anim_tick < kBootAnimFrames) {
        ++s_boot_anim_tick;
        request_redraw_rect(0, 0, kScreenWidth, kScreenHeight);
    }

    if ((s_ticks % kTicksPerSecondEstimate) == 0U) {
        request_redraw_clock();
        if (s_max_idle_spins == 0U) {
            s_max_idle_spins = 1U;
        }
        if (s_last_idle_spins > s_max_idle_spins) {
            s_max_idle_spins = s_last_idle_spins;
        }
        uint8_t cpu = 0;
        if (s_last_idle_spins <= s_max_idle_spins) {
            const uint32_t idle_pct = (s_last_idle_spins * 100U) / s_max_idle_spins;
            cpu = (uint8_t)(idle_pct >= 100U ? 0U : (100U - idle_pct));
        }
        const size_t cap = fs_ramdisk_capacity();
        const size_t used = fs_ramdisk_used();
        uint8_t mem = 0;
        if (cap > 0) {
            uint32_t mem_pct = (uint32_t)((used * 100U) / cap);
            if (mem_pct > 100U) mem_pct = 100U;
            mem = (uint8_t)mem_pct;
        }
        perf_push_sample(cpu, mem);
        if (s_app_windows[APP_PERFORMANCE].open && !s_app_windows[APP_PERFORMANCE].minimized) {
            request_redraw();
        }
    }

    process_queued_keys();
    process_pending_shell_command();

    ++s_autosave_ticks;
    if (s_autosave_ticks >= kTicksPerSecondEstimate * 5U) {
        s_autosave_ticks = 0;
        bool saved = false;
        if (s_notes_dirty) {
            notes_save();
            saved = true;
        }
        if (s_editor_dirty) {
            editor_save();
            saved = true;
        }
        if (saved) {
            log_push_line("Autosaved notes/editor.");
            request_redraw_log_and_status();
        }
    }

    apply_mouse_frame_state();

    ++s_blink_frame_counter;
    if (s_blink_frame_counter >= kCursorBlinkFrames) {
        s_blink_frame_counter = 0;
        s_input_cursor_visible = !s_input_cursor_visible;
        request_redraw_input();
    }

    if (!s_needs_redraw) {
        return;
    }

    redraw();
    s_needs_redraw = false;
}

static bool text_insert_char(char* text, size_t* len, size_t cap, size_t* cursor, char c) {
    if (text == NULL || len == NULL || cursor == NULL || *len + 1 >= cap) {
        return false;
    }
    if (*cursor > *len) {
        *cursor = *len;
    }

    for (size_t i = *len; i > *cursor; --i) {
        text[i] = text[i - 1];
    }
    text[*cursor] = c;
    ++(*len);
    ++(*cursor);
    text[*len] = '\0';
    return true;
}

static bool text_backspace_char(char* text, size_t* len, size_t* cursor) {
    if (text == NULL || len == NULL || cursor == NULL || *len == 0 || *cursor == 0) {
        return false;
    }
    if (*cursor > *len) {
        *cursor = *len;
    }

    const size_t at = *cursor - 1;
    for (size_t i = at; i < *len; ++i) {
        text[i] = text[i + 1];
    }
    --(*len);
    --(*cursor);
    return true;
}

static bool handle_focused_editor_key(char c) {
    char* text = NULL;
    size_t* len = NULL;
    size_t cap = 0;
    size_t* cursor = NULL;
    bool* dirty = NULL;

    if (s_notes_focused) {
        text = s_notes_text;
        len = &s_notes_len;
        cap = sizeof(s_notes_text);
        cursor = &s_notes_cursor;
        dirty = &s_notes_dirty;
    } else if (s_editor_focused) {
        text = s_editor_text;
        len = &s_editor_len;
        cap = sizeof(s_editor_text);
        cursor = &s_editor_cursor;
        dirty = &s_editor_dirty;
    } else {
        return false;
    }

    bool changed = false;
    if (c == '\b') {
        changed = text_backspace_char(text, len, cursor);
    } else if (c == '\n') {
        changed = text_insert_char(text, len, cap, cursor, '\n');
    } else if (c == '\t') {
        changed = text_insert_char(text, len, cap, cursor, ' ');
        changed = text_insert_char(text, len, cap, cursor, ' ') || changed;
        changed = text_insert_char(text, len, cap, cursor, ' ') || changed;
        changed = text_insert_char(text, len, cap, cursor, ' ') || changed;
    } else if (c >= 32 && c <= 126) {
        changed = text_insert_char(text, len, cap, cursor, c);
    }

    if (changed && dirty != NULL) {
        *dirty = true;
        request_redraw();
    }
    return true;
}

void desktop_handle_key(char c) {
    if (s_sleeping) {
        s_sleeping = false;
        request_redraw();
        return;
    }

    if (!session_logged_in()) {
        (void)login_handle_key(c);
        s_input_cursor_visible = true;
        s_blink_frame_counter = 0;
        return;
    }

    if (s_start_menu_open && (s_start_search_focused || c == '\b' || c == '\n' || c == 27 || (c >= 32 && c <= 126))) {
        if (c == 27) {
            s_start_menu_open = false;
            start_menu_reset_search();
            request_redraw();
            return;
        }

        s_start_search_focused = true;

        if (c == '\n') {
            const int item = start_menu_first_visible_item();
            if (item >= 0) {
                execute_start_menu_item(item);
            } else {
                request_redraw();
            }
            return;
        }

        if (c == '\b') {
            if (s_start_search_len > 0) {
                --s_start_search_len;
                s_start_search[s_start_search_len] = '\0';
                request_redraw();
            }
            return;
        }

        if (c >= 32 && c <= 126 && s_start_search_len + 1 < sizeof(s_start_search)) {
            s_start_search[s_start_search_len++] = c;
            s_start_search[s_start_search_len] = '\0';
            request_redraw();
        }
        return;
    }

    if (handle_focused_editor_key(c)) {
        s_input_cursor_visible = true;
        s_blink_frame_counter = 0;
        return;
    }

    if (c == '\n') {
        s_input_line[s_input_len] = '\0';
        if (s_input_len > 0 && !s_has_pending_command) {
            copy_str(s_pending_command, sizeof(s_pending_command), s_input_line);
            s_has_pending_command = true;

            char echo[kLogLineLen];
            copy_str(echo, sizeof(echo), "> ");
            size_t i = cstr_len(echo);
            size_t j = 0;
            while (s_input_line[j] != '\0' && i + 1 < sizeof(echo)) {
                echo[i++] = s_input_line[j++];
            }
            echo[i] = '\0';
            log_push_line(echo);
        }

        s_input_len = 0;
        s_input_line[0] = '\0';
        s_input_cursor_visible = true;
        s_blink_frame_counter = 0;
        request_redraw_input();
        request_redraw_log_and_status();
        return;
    }

    if (c == '\b') {
        if (s_input_len > 0) {
            --s_input_len;
            s_input_line[s_input_len] = '\0';
            s_input_cursor_visible = true;
            s_blink_frame_counter = 0;
            request_redraw_input();
        }
        return;
    }

    if (c == '\t') {
        if (s_input_len + 4 < sizeof(s_input_line)) {
            s_input_line[s_input_len++] = ' ';
            s_input_line[s_input_len++] = ' ';
            s_input_line[s_input_len++] = ' ';
            s_input_line[s_input_len++] = ' ';
            s_input_line[s_input_len] = '\0';
            s_input_cursor_visible = true;
            s_blink_frame_counter = 0;
            request_redraw_input();
        }
        return;
    }

    if (c >= 32 && c <= 126 && s_input_len + 1 < sizeof(s_input_line)) {
        s_input_line[s_input_len++] = c;
        s_input_line[s_input_len] = '\0';
        s_input_cursor_visible = true;
        s_blink_frame_counter = 0;
        request_redraw_input();
    }
}

static void process_queued_keys(void) {
    char c = 0;
    int budget = 32;
    while (budget-- > 0 && key_queue_pop(&c)) {
        desktop_handle_key(c);
    }
}

static void process_pending_shell_command(void) {
    if (!session_logged_in() || !s_has_pending_command) {
        return;
    }

    char cmd[sizeof(s_pending_command)];
    copy_str(cmd, sizeof(cmd), s_pending_command);
    s_has_pending_command = false;
    s_pending_command[0] = '\0';

    cli_action action = cli_execute(cmd);
    if (action != CLI_ACTION_NONE) {
        if (s_pending_kernel_action == CLI_ACTION_NONE) {
            s_pending_kernel_action = action;
        } else {
            log_push_line("Kernel action pending: command deferred.");
            request_redraw_log_and_status();
        }
    }
}

bool desktop_consume_kernel_action(cli_action* out_action) {
    if (out_action == NULL || s_pending_kernel_action == CLI_ACTION_NONE) {
        return false;
    }

    *out_action = s_pending_kernel_action;
    s_pending_kernel_action = CLI_ACTION_NONE;
    return true;
}
