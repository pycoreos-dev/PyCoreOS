#include "doom/raycast.h"

#include "drivers/framebuffer.h"
#include "drivers/keyboard.h"
#include "kernel/timing.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    kMaxScreenW = 800,
    kMaxScreenH = 600,
    kMapW = 24,
    kMapH = 24,
    kFpShift = 8,
    kFpOne = 1 << kFpShift,
    kRayInf = 1 << 28,
    kMoveStep = kFpOne / 5,
    kStrafeStep = kFpOne / 6,
    kRotSin = 31,  /* ~sin(7 deg) * 256 */
    kRotCos = 254, /* ~cos(7 deg) * 256 */
};

static uint32_t s_frame[kMaxScreenW * kMaxScreenH];

/* 24x24 world map with wall IDs for color variation. */
static const uint8_t kWorld[kMapH][kMapW] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,2,2,0,0,0,0,0,3,3,3,0,0,0,0,0,4,4,4,0,0,0,1},
    {1,0,2,0,0,0,0,0,0,3,0,3,0,0,0,0,0,4,0,4,0,0,0,1},
    {1,0,2,2,0,0,5,5,0,3,3,3,0,0,0,0,0,4,4,4,0,0,0,1},
    {1,0,0,0,0,0,5,0,0,0,0,0,0,2,2,2,0,0,0,0,0,6,0,1},
    {1,0,0,0,0,0,5,5,5,0,0,0,0,2,0,2,0,0,0,0,0,6,0,1},
    {1,0,3,3,3,0,0,0,0,0,0,0,0,2,2,2,0,0,0,0,0,6,0,1},
    {1,0,3,0,3,0,0,0,0,4,4,4,0,0,0,0,0,7,7,7,0,0,0,1},
    {1,0,3,3,3,0,0,0,0,4,0,4,0,0,0,0,0,7,0,7,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,4,4,4,0,0,0,0,0,7,7,7,0,0,0,1},
    {1,0,0,0,0,0,6,6,6,0,0,0,0,5,5,5,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,6,0,6,0,0,0,0,5,0,5,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,6,6,6,0,0,0,0,5,5,5,0,0,4,4,4,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,2,2,0,0,0,0,0,0,4,0,4,0,0,1},
    {1,0,7,7,7,0,0,0,0,0,2,0,0,0,0,0,0,0,4,4,4,0,0,1},
    {1,0,7,0,7,0,0,0,0,0,2,2,0,0,6,6,6,0,0,0,0,0,0,1},
    {1,0,7,7,7,0,0,0,0,0,0,0,0,0,6,0,6,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,3,3,3,0,0,0,0,6,6,6,0,0,5,5,5,0,1},
    {1,0,0,0,0,0,0,3,0,3,0,0,0,0,0,0,0,0,0,5,0,5,0,1},
    {1,0,0,0,0,0,0,3,3,3,0,0,0,0,0,0,0,0,0,5,5,5,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
};

static int32_t s_pos_x = (12 << kFpShift) + (kFpOne / 2);
static int32_t s_pos_y = (12 << kFpShift) + (kFpOne / 2);
static int32_t s_dir_x = -kFpOne;
static int32_t s_dir_y = 0;
static int32_t s_plane_x = 0;
static int32_t s_plane_y = 169; /* 0.66 * 256 */

static inline int32_t fp_abs(int32_t v) {
    return (v < 0) ? -v : v;
}

static inline int32_t fp_mul(int32_t a, int32_t b) {
    return (a * b) >> kFpShift;
}

static inline int32_t fp_div(int32_t a, int32_t b) {
    if (b == 0) {
        return (a < 0) ? -kRayInf : kRayInf;
    }
    return (a << kFpShift) / b;
}

static inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16U) | ((uint32_t)g << 8U) | (uint32_t)b;
}

static uint32_t shade_color(uint32_t base, int shade) {
    if (shade < 24) {
        shade = 24;
    }
    if (shade > 255) {
        shade = 255;
    }
    const int r = (int)((base >> 16U) & 0xFFU);
    const int g = (int)((base >> 8U) & 0xFFU);
    const int b = (int)(base & 0xFFU);
    return rgb((uint8_t)((r * shade) >> 8), (uint8_t)((g * shade) >> 8), (uint8_t)((b * shade) >> 8));
}

static uint32_t wall_base_color(uint8_t wall_id) {
    switch (wall_id & 7U) {
        case 1: return 0xB04739;
        case 2: return 0x3E6DC4;
        case 3: return 0x4A9B51;
        case 4: return 0x9B8D45;
        case 5: return 0x8F4AC0;
        case 6: return 0x58A3A3;
        case 7: return 0xB7672B;
        default: return 0xA0A0A0;
    }
}

static bool map_is_wall_cell(int x, int y) {
    if (x < 0 || y < 0 || x >= kMapW || y >= kMapH) {
        return true;
    }
    return kWorld[y][x] != 0;
}

static bool collides_at(int32_t fx, int32_t fy) {
    const int32_t radius = kFpOne / 6;
    const int x0 = (fx - radius) >> kFpShift;
    const int x1 = (fx + radius) >> kFpShift;
    const int y0 = (fy - radius) >> kFpShift;
    const int y1 = (fy + radius) >> kFpShift;
    return map_is_wall_cell(x0, y0) || map_is_wall_cell(x1, y0) ||
           map_is_wall_cell(x0, y1) || map_is_wall_cell(x1, y1);
}

static void try_move(int32_t dx, int32_t dy) {
    const int32_t nx = s_pos_x + dx;
    if (!collides_at(nx, s_pos_y)) {
        s_pos_x = nx;
    }

    const int32_t ny = s_pos_y + dy;
    if (!collides_at(s_pos_x, ny)) {
        s_pos_y = ny;
    }
}

static void rotate_view(bool right) {
    const int32_t sin_v = right ? -kRotSin : kRotSin;
    const int32_t cos_v = kRotCos;

    const int32_t old_dir_x = s_dir_x;
    s_dir_x = fp_mul(s_dir_x, cos_v) - fp_mul(s_dir_y, sin_v);
    s_dir_y = fp_mul(old_dir_x, sin_v) + fp_mul(s_dir_y, cos_v);

    const int32_t old_plane_x = s_plane_x;
    s_plane_x = fp_mul(s_plane_x, cos_v) - fp_mul(s_plane_y, sin_v);
    s_plane_y = fp_mul(old_plane_x, sin_v) + fp_mul(s_plane_y, cos_v);
}

static void draw_background(int screen_w, int screen_h, uint32_t tick) {
    if (screen_h <= 0 || screen_w <= 0) {
        return;
    }
    const int half = screen_h / 2;

    for (int y = 0; y < half; ++y) {
        const int t = (y * 255) / (half > 0 ? half : 1);
        const uint8_t r = (uint8_t)(18 + (t / 12));
        const uint8_t g = (uint8_t)(36 + (t / 6));
        const uint8_t b = (uint8_t)(74 + (t / 4) + ((int)(tick & 7U)));
        const uint32_t c = rgb(r, g, b);
        uint32_t* row = &s_frame[(size_t)y * kMaxScreenW];
        for (int x = 0; x < screen_w; ++x) {
            row[x] = c;
        }
    }

    for (int y = half; y < screen_h; ++y) {
        const int t = ((y - half) * 255) / ((screen_h - half) > 0 ? (screen_h - half) : 1);
        const uint8_t r = (uint8_t)(50 - (t / 7));
        const uint8_t g = (uint8_t)(44 - (t / 8));
        const uint8_t b = (uint8_t)(36 - (t / 9));
        const uint32_t c = rgb(r, g, b);
        uint32_t* row = &s_frame[(size_t)y * kMaxScreenW];
        for (int x = 0; x < screen_w; ++x) {
            row[x] = c;
        }
    }
}

static void draw_weapon_overlay(int screen_w, int screen_h) {
    const int weapon_w = screen_w / 5;
    const int weapon_h = screen_h / 4;
    const int x0 = (screen_w - weapon_w) / 2;
    const int y0 = screen_h - weapon_h;
    const int x1 = x0 + weapon_w;
    const int y1 = y0 + weapon_h;

    for (int y = y0; y < y1; ++y) {
        if (y < 0 || y >= screen_h) {
            continue;
        }
        uint32_t* row = &s_frame[(size_t)y * kMaxScreenW];
        for (int x = x0; x < x1; ++x) {
            if (x < 0 || x >= screen_w) {
                continue;
            }
            const int dy = y - y0;
            const int dx = x - x0;
            uint32_t c = 0x2A2A2A;
            if (dy > weapon_h / 3 && dy < (weapon_h * 5) / 6 && dx > weapon_w / 4 && dx < (weapon_w * 3) / 4) {
                c = 0x707070;
            }
            if (dy > (weapon_h * 5) / 6) {
                c = 0x1A1A1A;
            }
            row[x] = c;
        }
    }
}

static void render_scene(int screen_w, int screen_h, uint32_t tick) {
    draw_background(screen_w, screen_h, tick);

    for (int x = 0; x < screen_w; ++x) {
        const int32_t camera_x = (((x * 2) << kFpShift) / (screen_w > 0 ? screen_w : 1)) - kFpOne;
        const int32_t ray_dir_x = s_dir_x + fp_mul(s_plane_x, camera_x);
        const int32_t ray_dir_y = s_dir_y + fp_mul(s_plane_y, camera_x);

        int map_x = s_pos_x >> kFpShift;
        int map_y = s_pos_y >> kFpShift;

        const int32_t delta_x = (ray_dir_x == 0) ? kRayInf : fp_abs(fp_div(kFpOne, ray_dir_x));
        const int32_t delta_y = (ray_dir_y == 0) ? kRayInf : fp_abs(fp_div(kFpOne, ray_dir_y));

        int step_x = 1;
        int step_y = 1;
        int32_t side_x = 0;
        int32_t side_y = 0;

        if (ray_dir_x < 0) {
            step_x = -1;
            side_x = fp_mul(s_pos_x - (map_x << kFpShift), delta_x);
        } else {
            side_x = fp_mul(((map_x + 1) << kFpShift) - s_pos_x, delta_x);
        }

        if (ray_dir_y < 0) {
            step_y = -1;
            side_y = fp_mul(s_pos_y - (map_y << kFpShift), delta_y);
        } else {
            side_y = fp_mul(((map_y + 1) << kFpShift) - s_pos_y, delta_y);
        }

        int side = 0;
        int hit = 0;
        uint8_t wall_id = 1;
        for (int guard = 0; guard < 64; ++guard) {
            if (side_x < side_y) {
                side_x += delta_x;
                map_x += step_x;
                side = 0;
            } else {
                side_y += delta_y;
                map_y += step_y;
                side = 1;
            }

            if (map_x < 0 || map_y < 0 || map_x >= kMapW || map_y >= kMapH) {
                hit = 1;
                wall_id = 1;
                break;
            }

            wall_id = kWorld[map_y][map_x];
            if (wall_id != 0) {
                hit = 1;
                break;
            }
        }

        if (!hit) {
            continue;
        }

        int32_t perp = (side == 0) ? (side_x - delta_x) : (side_y - delta_y);
        if (perp < (kFpOne / 16)) {
            perp = kFpOne / 16;
        }

        int line_h = (screen_h << kFpShift) / perp;
        if (line_h < 1) {
            line_h = 1;
        }

        int draw_start = (-line_h / 2) + (screen_h / 2);
        int draw_end = (line_h / 2) + (screen_h / 2);
        if (draw_start < 0) {
            draw_start = 0;
        }
        if (draw_end >= screen_h) {
            draw_end = screen_h - 1;
        }

        int dist_i = perp >> kFpShift;
        int shade = 255 - dist_i * 14;
        if (side != 0) {
            shade = (shade * 3) / 4;
        }
        const uint32_t color = shade_color(wall_base_color(wall_id), shade);

        for (int y = draw_start; y <= draw_end; ++y) {
            s_frame[(size_t)y * kMaxScreenW + (size_t)x] = color;
        }
    }

    draw_weapon_overlay(screen_w, screen_h);
}

static void process_input(bool* running) {
    char c = 0;
    while (keyboard_read_char(&c)) {
        switch (c) {
            case 27:
            case 'q':
            case 'Q':
            case 'x':
            case 'X':
                *running = false;
                break;
            case 'w':
            case 'W':
                try_move(fp_mul(s_dir_x, kMoveStep), fp_mul(s_dir_y, kMoveStep));
                break;
            case 's':
            case 'S':
                try_move(-fp_mul(s_dir_x, kMoveStep), -fp_mul(s_dir_y, kMoveStep));
                break;
            case 'a':
            case 'A':
                try_move(-fp_mul(s_dir_y, kStrafeStep), fp_mul(s_dir_x, kStrafeStep));
                break;
            case 'd':
            case 'D':
                try_move(fp_mul(s_dir_y, kStrafeStep), -fp_mul(s_dir_x, kStrafeStep));
                break;
            case 'j':
            case 'J':
                rotate_view(false);
                break;
            case 'l':
            case 'L':
                rotate_view(true);
                break;
            default:
                break;
        }
    }
}

void raycast_run(void) {
    if (!framebuffer_ready()) {
        return;
    }

    int screen_w = (int)framebuffer_width();
    int screen_h = (int)framebuffer_height();
    if (screen_w <= 0 || screen_h <= 0) {
        return;
    }
    if (screen_w > kMaxScreenW) {
        screen_w = kMaxScreenW;
    }
    if (screen_h > kMaxScreenH) {
        screen_h = kMaxScreenH;
    }

    bool running = true;
    uint32_t tick = 0;
    while (running) {
        process_input(&running);
        render_scene(screen_w, screen_h, tick);
        framebuffer_present_argb8888(s_frame, (uint32_t)kMaxScreenW);
        timing_sleep_ms(16);
        ++tick;
    }
}
