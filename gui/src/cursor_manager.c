#include "gui/cursor_manager.h"

#include "drivers/framebuffer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    kScreenWidth = 1280,
    kScreenHeight = 720,
    kCursorSpriteSize = 32,
    kCursorPixelCount = kCursorSpriteSize * kCursorSpriteSize,
    kArrowSpriteSize = 16,
};

typedef enum cursor_type {
    CURSOR_TYPE_ARROW = 0,
    CURSOR_TYPE_IBEAM = 1,
    CURSOR_TYPE_HAND = 2,
    CURSOR_TYPE_RESIZE_EW = 3,
    CURSOR_TYPE_RESIZE_NS = 4,
    CURSOR_TYPE_RESIZE_NWSE = 5,
    CURSOR_TYPE_RESIZE_NESW = 6,
    CURSOR_TYPE_COUNT = 7,
} cursor_type;

typedef struct cursor_sprite {
    int width;
    int height;
    int hot_x;
    int hot_y;
    uint32_t pixels[kCursorPixelCount];
} cursor_sprite;

static int s_screen_w = 0;
static int s_screen_h = 0;

static const uint32_t* s_scene = NULL;
static uint32_t s_scene_pitch = 0;

static cursor_sprite s_sprites[CURSOR_TYPE_COUNT];

static int s_target_x = 0;
static int s_target_y = 0;
static cursor_type s_target_type = CURSOR_TYPE_ARROW;

static int s_drawn_x = 0;
static int s_drawn_y = 0;
static cursor_type s_drawn_type = CURSOR_TYPE_ARROW;
static bool s_drawn = false;
static bool s_dirty = false;

static uint32_t s_saved_bg_a[kCursorPixelCount];
static uint32_t s_saved_bg_b[kCursorPixelCount];
static uint32_t* s_saved_bg_drawn = s_saved_bg_a;
static uint32_t* s_saved_bg_target = s_saved_bg_b;

static const uint32_t kCursorOutlineRgb = 0x000000;
static const uint32_t kCursorFillRgb = 0xFFFFFF;

static uint32_t make_argb(uint8_t a, uint32_t rgb) {
    return ((uint32_t)a << 24U) | (rgb & 0x00FFFFFFU);
}

static uint32_t blend_rgb_with_argb(uint32_t bg_rgb, uint32_t fg_argb) {
    const uint32_t a = (fg_argb >> 24U) & 0xFFU;
    if (a == 0) {
        return bg_rgb & 0x00FFFFFFU;
    }
    if (a == 255U) {
        return fg_argb & 0x00FFFFFFU;
    }

    const uint32_t br = (bg_rgb >> 16U) & 0xFFU;
    const uint32_t bg = (bg_rgb >> 8U) & 0xFFU;
    const uint32_t bb = bg_rgb & 0xFFU;
    const uint32_t fr = (fg_argb >> 16U) & 0xFFU;
    const uint32_t fg = (fg_argb >> 8U) & 0xFFU;
    const uint32_t fb = fg_argb & 0xFFU;

    const uint32_t r = (br * (255U - a) + fr * a) / 255U;
    const uint32_t g = (bg * (255U - a) + fg * a) / 255U;
    const uint32_t b = (bb * (255U - a) + fb * a) / 255U;
    return (r << 16U) | (g << 8U) | b;
}

static void sprite_clear(cursor_type type) {
    for (size_t i = 0; i < kCursorPixelCount; ++i) {
        s_sprites[type].pixels[i] = 0;
    }
}

static void sprite_plot(cursor_type type, int x, int y, uint8_t alpha, uint32_t rgb) {
    if (x < 0 || y < 0 || x >= kCursorSpriteSize || y >= kCursorSpriteSize) {
        return;
    }

    cursor_sprite* s = &s_sprites[type];
    const size_t idx = (size_t)y * kCursorSpriteSize + (size_t)x;
    const uint32_t prev = s->pixels[idx];
    const uint8_t prev_a = (uint8_t)(prev >> 24U);
    if (alpha < prev_a) {
        return;
    }
    s->pixels[idx] = make_argb(alpha, rgb);
}

static void sprite_diag_line(cursor_type type, int x0, int y0, int x1, int y1, uint8_t alpha, uint32_t rgb) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    const int sx = (dx >= 0) ? 1 : -1;
    const int sy = (dy >= 0) ? 1 : -1;
    dx = (dx >= 0) ? dx : -dx;
    dy = (dy >= 0) ? dy : -dy;
    int err = (dx > dy ? dx : -dy) / 2;

    for (;;) {
        sprite_plot(type, x0, y0, alpha, rgb);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int e2 = err;
        if (e2 > -dx) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dy) {
            err += dx;
            y0 += sy;
        }
    }
}

static void sprite_plot_pattern(cursor_type type, int origin_x, int origin_y, const char* const* rows, int row_count) {
    for (int y = 0; y < row_count; ++y) {
        const char* row = rows[y];
        for (int x = 0; row[x] != '\0'; ++x) {
            const char px = row[x];
            if (px == 'X') {
                sprite_plot(type, origin_x + x, origin_y + y, 255, kCursorOutlineRgb);
            } else if (px == 'O') {
                sprite_plot(type, origin_x + x, origin_y + y, 255, kCursorFillRgb);
            }
        }
    }
}

static void build_arrow_sprite(void) {
    cursor_sprite* s = &s_sprites[CURSOR_TYPE_ARROW];
    s->width = kArrowSpriteSize;
    s->height = kArrowSpriteSize;
    s->hot_x = 0;
    s->hot_y = 0;
    sprite_clear(CURSOR_TYPE_ARROW);

    static const char* const arrow_rows[] = {
        "X...............",
        "XX..............",
        "XOX.............",
        "XOOX............",
        "XOOOX...........",
        "XOOOOX..........",
        "XOOOOOX.........",
        "XOOOOOOX........",
        "XOOOOOOOX.......",
        "XOOOOOOOOX......",
        "XOOOOXXXXX......",
        "XOOXOOX.........",
        "XOX.XOOX........",
        "XX..XOOX........",
        "X....XOOX.......",
        "......XX........",
    };

    sprite_plot_pattern(CURSOR_TYPE_ARROW, 0, 0, arrow_rows, (int)(sizeof(arrow_rows) / sizeof(arrow_rows[0])));
}

static void build_ibeam_sprite(void) {
    cursor_sprite* s = &s_sprites[CURSOR_TYPE_IBEAM];
    s->width = kCursorSpriteSize;
    s->height = kCursorSpriteSize;
    s->hot_x = 15;
    s->hot_y = 15;
    sprite_clear(CURSOR_TYPE_IBEAM);

    static const char* const ibeam_rows[] = {
        "..XXXXXXXXX..",
        ".XOOOOOOOOOX.",
        ".XOOOOOOOOOX.",
        "....XOOOX....",
        "....XOOOX....",
        "....XOOOX....",
        "....XOOOX....",
        "....XOOOX....",
        "....XOOOX....",
        "....XOOOX....",
        "....XOOOX....",
        "....XOOOX....",
        "....XOOOX....",
        "....XOOOX....",
        ".XOOOOOOOOOX.",
        ".XOOOOOOOOOX.",
        "..XXXXXXXXX..",
    };

    sprite_plot_pattern(CURSOR_TYPE_IBEAM, 9, 7, ibeam_rows, (int)(sizeof(ibeam_rows) / sizeof(ibeam_rows[0])));
}

static void build_hand_sprite(void) {
    cursor_sprite* s = &s_sprites[CURSOR_TYPE_HAND];
    s->width = kCursorSpriteSize;
    s->height = kCursorSpriteSize;
    s->hot_x = 9;
    s->hot_y = 4;
    sprite_clear(CURSOR_TYPE_HAND);

    static const char* const hand_rows[] = {
        "....XX..........",
        "...XOOX.........",
        "...XOOX.........",
        "...XOOX.........",
        "...XOOX.........",
        "...XOOX.........",
        "...XOOX.........",
        "...XOOX.........",
        "...XOOXXX.......",
        "...XOOOOOX......",
        "...XOOXOOOX.....",
        "...XOOXOOOX.....",
        "...XOOXOOOX.....",
        "...XOOXOOOX.....",
        "...XOOXOOOX.....",
        "...XOOOOOOOX....",
        "...XOOOOOOOX....",
        "....XOOOOOOX....",
        ".....XOOOOOX....",
        "......XXXXX.....",
    };

    sprite_plot_pattern(CURSOR_TYPE_HAND, 5, 4, hand_rows, (int)(sizeof(hand_rows) / sizeof(hand_rows[0])));
}

static void build_resize_ew_sprite(void) {
    cursor_sprite* s = &s_sprites[CURSOR_TYPE_RESIZE_EW];
    s->width = kCursorSpriteSize;
    s->height = kCursorSpriteSize;
    s->hot_x = 15;
    s->hot_y = 15;
    sprite_clear(CURSOR_TYPE_RESIZE_EW);

    static const char* const resize_ew_rows[] = {
        "....X.........X....",
        "...XX.........XX...",
        "..XOX.........XOX..",
        ".XOOXXXXXXXXXXXOOX.",
        "XOOOOOOOOOOOOOOOOOX",
        ".XOOXXXXXXXXXXXOOX.",
        "..XOX.........XOX..",
        "...XX.........XX...",
        "....X.........X....",
    };

    sprite_plot_pattern(CURSOR_TYPE_RESIZE_EW, 6, 11, resize_ew_rows, (int)(sizeof(resize_ew_rows) / sizeof(resize_ew_rows[0])));
}

static void build_resize_ns_sprite(void) {
    cursor_sprite* s = &s_sprites[CURSOR_TYPE_RESIZE_NS];
    s->width = kCursorSpriteSize;
    s->height = kCursorSpriteSize;
    s->hot_x = 15;
    s->hot_y = 15;
    sprite_clear(CURSOR_TYPE_RESIZE_NS);

    static const char* const resize_ns_rows[] = {
        "....X....",
        "...XXX...",
        "..XOXOX..",
        ".XOOXOOX.",
        "XOOOXOOOX",
        "..XOOOX..",
        "..XOOOX..",
        "..XOOOX..",
        "..XOOOX..",
        "..XOOOX..",
        "..XOOOX..",
        "..XOOOX..",
        "..XOOOX..",
        "..XOOOX..",
        "XOOOXOOOX",
        ".XOOXOOX.",
        "..XOXOX..",
        "...XXX...",
        "....X....",
    };

    sprite_plot_pattern(CURSOR_TYPE_RESIZE_NS, 11, 6, resize_ns_rows, (int)(sizeof(resize_ns_rows) / sizeof(resize_ns_rows[0])));
}

static void build_resize_nwse_sprite(void) {
    cursor_sprite* s = &s_sprites[CURSOR_TYPE_RESIZE_NWSE];
    s->width = kCursorSpriteSize;
    s->height = kCursorSpriteSize;
    s->hot_x = 15;
    s->hot_y = 15;
    sprite_clear(CURSOR_TYPE_RESIZE_NWSE);

    sprite_diag_line(CURSOR_TYPE_RESIZE_NWSE, 7, 7, 24, 24, 255, kCursorOutlineRgb);
    sprite_diag_line(CURSOR_TYPE_RESIZE_NWSE, 8, 7, 24, 23, 255, kCursorOutlineRgb);
    sprite_diag_line(CURSOR_TYPE_RESIZE_NWSE, 7, 8, 23, 24, 255, kCursorOutlineRgb);
    sprite_diag_line(CURSOR_TYPE_RESIZE_NWSE, 8, 8, 23, 23, 255, kCursorFillRgb);

    for (int i = 0; i < 4; ++i) {
        sprite_plot(CURSOR_TYPE_RESIZE_NWSE, 7 + i, 7, 255, kCursorOutlineRgb);
        sprite_plot(CURSOR_TYPE_RESIZE_NWSE, 7, 7 + i, 255, kCursorOutlineRgb);
        sprite_plot(CURSOR_TYPE_RESIZE_NWSE, 24 - i, 24, 255, kCursorOutlineRgb);
        sprite_plot(CURSOR_TYPE_RESIZE_NWSE, 24, 24 - i, 255, kCursorOutlineRgb);
    }
    for (int i = 1; i < 3; ++i) {
        sprite_plot(CURSOR_TYPE_RESIZE_NWSE, 7 + i, 8, 255, kCursorFillRgb);
        sprite_plot(CURSOR_TYPE_RESIZE_NWSE, 8, 7 + i, 255, kCursorFillRgb);
        sprite_plot(CURSOR_TYPE_RESIZE_NWSE, 24 - i, 23, 255, kCursorFillRgb);
        sprite_plot(CURSOR_TYPE_RESIZE_NWSE, 23, 24 - i, 255, kCursorFillRgb);
    }
}

static void build_resize_nesw_sprite(void) {
    cursor_sprite* s = &s_sprites[CURSOR_TYPE_RESIZE_NESW];
    s->width = kCursorSpriteSize;
    s->height = kCursorSpriteSize;
    s->hot_x = 15;
    s->hot_y = 15;
    sprite_clear(CURSOR_TYPE_RESIZE_NESW);

    sprite_diag_line(CURSOR_TYPE_RESIZE_NESW, 24, 7, 7, 24, 255, kCursorOutlineRgb);
    sprite_diag_line(CURSOR_TYPE_RESIZE_NESW, 23, 7, 7, 23, 255, kCursorOutlineRgb);
    sprite_diag_line(CURSOR_TYPE_RESIZE_NESW, 24, 8, 8, 24, 255, kCursorOutlineRgb);
    sprite_diag_line(CURSOR_TYPE_RESIZE_NESW, 23, 8, 8, 23, 255, kCursorFillRgb);

    for (int i = 0; i < 4; ++i) {
        sprite_plot(CURSOR_TYPE_RESIZE_NESW, 24 - i, 7, 255, kCursorOutlineRgb);
        sprite_plot(CURSOR_TYPE_RESIZE_NESW, 24, 7 + i, 255, kCursorOutlineRgb);
        sprite_plot(CURSOR_TYPE_RESIZE_NESW, 7 + i, 24, 255, kCursorOutlineRgb);
        sprite_plot(CURSOR_TYPE_RESIZE_NESW, 7, 24 - i, 255, kCursorOutlineRgb);
    }
    for (int i = 1; i < 3; ++i) {
        sprite_plot(CURSOR_TYPE_RESIZE_NESW, 24 - i, 8, 255, kCursorFillRgb);
        sprite_plot(CURSOR_TYPE_RESIZE_NESW, 23, 7 + i, 255, kCursorFillRgb);
        sprite_plot(CURSOR_TYPE_RESIZE_NESW, 8 + i, 23, 255, kCursorFillRgb);
        sprite_plot(CURSOR_TYPE_RESIZE_NESW, 8, 24 - i, 255, kCursorFillRgb);
    }
}

static void build_sprites(void) {
    build_arrow_sprite();
    build_ibeam_sprite();
    build_hand_sprite();
    build_resize_ew_sprite();
    build_resize_ns_sprite();
    build_resize_nwse_sprite();
    build_resize_nesw_sprite();
}

static cursor_type type_for_context(cursor_context context) {
    if (context == CURSOR_CONTEXT_TEXT) {
        return CURSOR_TYPE_IBEAM;
    }
    if (context == CURSOR_CONTEXT_CLICKABLE) {
        return CURSOR_TYPE_HAND;
    }
    if (context == CURSOR_CONTEXT_RESIZE_EW) {
        return CURSOR_TYPE_RESIZE_EW;
    }
    if (context == CURSOR_CONTEXT_RESIZE_NS) {
        return CURSOR_TYPE_RESIZE_NS;
    }
    if (context == CURSOR_CONTEXT_RESIZE_NWSE) {
        return CURSOR_TYPE_RESIZE_NWSE;
    }
    if (context == CURSOR_CONTEXT_RESIZE_NESW) {
        return CURSOR_TYPE_RESIZE_NESW;
    }
    return CURSOR_TYPE_ARROW;
}

static int sprite_left(const cursor_sprite* s, int x) {
    return x - s->hot_x;
}

static int sprite_top(const cursor_sprite* s, int y) {
    return y - s->hot_y;
}

static bool sprite_bounds_clipped(cursor_type type, int x, int y, int* out_x, int* out_y, int* out_w, int* out_h) {
    if (out_x == NULL || out_y == NULL || out_w == NULL || out_h == NULL) {
        return false;
    }

    const cursor_sprite* spr = &s_sprites[type];
    int left = sprite_left(spr, x);
    int top = sprite_top(spr, y);
    int right = left + spr->width;
    int bottom = top + spr->height;

    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > s_screen_w) right = s_screen_w;
    if (bottom > s_screen_h) bottom = s_screen_h;

    if (right <= left || bottom <= top) {
        return false;
    }

    *out_x = left;
    *out_y = top;
    *out_w = right - left;
    *out_h = bottom - top;
    return true;
}

static void swap_saved_buffers(void) {
    uint32_t* tmp = s_saved_bg_drawn;
    s_saved_bg_drawn = s_saved_bg_target;
    s_saved_bg_target = tmp;
}

static void restore_drawn_background(void) {
    if (!s_drawn) {
        return;
    }

    const cursor_sprite* spr = &s_sprites[s_drawn_type];
    const int left = sprite_left(spr, s_drawn_x);
    const int top = sprite_top(spr, s_drawn_y);

    for (int sy = 0; sy < spr->height; ++sy) {
        for (int sx = 0; sx < spr->width; ++sx) {
            const int gx = left + sx;
            const int gy = top + sy;
            if (gx < 0 || gy < 0 || gx >= s_screen_w || gy >= s_screen_h) {
                continue;
            }

            const size_t idx = (size_t)sy * kCursorSpriteSize + (size_t)sx;
            framebuffer_draw_pixel(gx, gy, s_saved_bg_drawn[idx]);
        }
    }
}

static void capture_background(cursor_type type, int x, int y, uint32_t* out_bg) {
    if (out_bg == NULL) {
        return;
    }

    if (s_scene == NULL || s_scene_pitch == 0) {
        for (size_t i = 0; i < kCursorPixelCount; ++i) {
            out_bg[i] = 0;
        }
        return;
    }

    const cursor_sprite* spr = &s_sprites[type];
    const int left = sprite_left(spr, x);
    const int top = sprite_top(spr, y);

    for (int sy = 0; sy < spr->height; ++sy) {
        for (int sx = 0; sx < spr->width; ++sx) {
            const int gx = left + sx;
            const int gy = top + sy;
            const size_t idx = (size_t)sy * kCursorSpriteSize + (size_t)sx;

            if (gx < 0 || gy < 0 || gx >= s_screen_w || gy >= s_screen_h) {
                out_bg[idx] = 0;
                continue;
            }
            out_bg[idx] = s_scene[(size_t)gy * s_scene_pitch + (size_t)gx] & 0x00FFFFFFU;
        }
    }
}

static void draw_cursor(cursor_type type, int x, int y, const uint32_t* bg) {
    if (bg == NULL) {
        return;
    }

    const cursor_sprite* spr = &s_sprites[type];
    const int left = sprite_left(spr, x);
    const int top = sprite_top(spr, y);

    for (int sy = 0; sy < spr->height; ++sy) {
        for (int sx = 0; sx < spr->width; ++sx) {
            const int gx = left + sx;
            const int gy = top + sy;
            if (gx < 0 || gy < 0 || gx >= s_screen_w || gy >= s_screen_h) {
                continue;
            }

            const size_t idx = (size_t)sy * kCursorSpriteSize + (size_t)sx;
            const uint32_t fg = spr->pixels[idx];
            const uint8_t a = (uint8_t)(fg >> 24U);
            if (a == 0) {
                continue;
            }

            const uint32_t out = blend_rgb_with_argb(bg[idx], fg);
            framebuffer_draw_pixel(gx, gy, out);
        }
    }
}

void cursor_manager_init(uint32_t screen_w, uint32_t screen_h) {
    (void)screen_w;
    (void)screen_h;
    s_screen_w = kScreenWidth;
    s_screen_h = kScreenHeight;
    s_scene = NULL;
    s_scene_pitch = 0;

    s_target_x = s_screen_w / 2;
    s_target_y = s_screen_h / 2;
    s_target_type = CURSOR_TYPE_ARROW;

    s_drawn_x = s_target_x;
    s_drawn_y = s_target_y;
    s_drawn_type = s_target_type;
    s_drawn = false;
    s_dirty = true;

    for (size_t i = 0; i < kCursorPixelCount; ++i) {
        s_saved_bg_a[i] = 0;
        s_saved_bg_b[i] = 0;
    }
    s_saved_bg_drawn = s_saved_bg_a;
    s_saved_bg_target = s_saved_bg_b;

    build_sprites();
}

void cursor_manager_set_scene(const uint32_t* scene_rgb, uint32_t scene_pitch_pixels) {
    s_scene = scene_rgb;
    s_scene_pitch = scene_pitch_pixels;
    s_dirty = true;
}

void cursor_manager_set_position(int x, int y) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= s_screen_w) x = s_screen_w - 1;
    if (y >= s_screen_h) y = s_screen_h - 1;

    if (x != s_target_x || y != s_target_y) {
        s_target_x = x;
        s_target_y = y;
        s_dirty = true;
    }
}

void cursor_manager_set_context(cursor_context context) {
    const cursor_type next = type_for_context(context);
    if (next != s_target_type) {
        s_target_type = next;
        s_dirty = true;
    }
}

void cursor_manager_on_scene_redraw(void) {
    if (s_scene == NULL || s_scene_pitch == 0) {
        return;
    }

    s_drawn_x = s_target_x;
    s_drawn_y = s_target_y;
    s_drawn_type = s_target_type;

    capture_background(s_drawn_type, s_drawn_x, s_drawn_y, s_saved_bg_target);
    draw_cursor(s_drawn_type, s_drawn_x, s_drawn_y, s_saved_bg_target);
    swap_saved_buffers();

    s_drawn = true;
    s_dirty = false;
}

void cursor_manager_step(void) {
    if (s_scene == NULL || s_scene_pitch == 0) {
        return;
    }

    if (!s_drawn) {
        cursor_manager_on_scene_redraw();
        return;
    }

    if (!s_dirty) {
        return;
    }

    restore_drawn_background();

    s_drawn_x = s_target_x;
    s_drawn_y = s_target_y;
    s_drawn_type = s_target_type;

    capture_background(s_drawn_type, s_drawn_x, s_drawn_y, s_saved_bg_target);
    draw_cursor(s_drawn_type, s_drawn_x, s_drawn_y, s_saved_bg_target);
    swap_saved_buffers();

    s_drawn = true;
    s_dirty = false;
}

bool cursor_manager_get_drawn_bounds(int* x, int* y, int* w, int* h) {
    if (!s_drawn) {
        return false;
    }
    return sprite_bounds_clipped(s_drawn_type, s_drawn_x, s_drawn_y, x, y, w, h);
}

bool cursor_manager_get_target_bounds(int* x, int* y, int* w, int* h) {
    return sprite_bounds_clipped(s_target_type, s_target_x, s_target_y, x, y, w, h);
}
