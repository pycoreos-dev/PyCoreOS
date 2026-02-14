#include "drivers/mouse.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    kPs2StatusPort = 0x64,
    kPs2DataPort = 0x60,
    kPs2StatusOutputFull = 0x01,
    kPs2StatusInputFull = 0x02,
    kPs2StatusAuxData = 0x20,

    kPs2CmdEnableAuxPort = 0xA8,
    kPs2CmdReadConfig = 0x20,
    kPs2CmdWriteConfig = 0x60,
    kPs2CmdWriteMouse = 0xD4,
    kPs2CfgIrq12Enable = 0x02,
    kPs2CfgDisableAuxClock = 0x20,

    kMouseAck = 0xFA,
    kMouseResend = 0xFE,
    kMouseCmdResetDefaults = 0xF6,
    kMouseCmdEnableStreaming = 0xF4,
    kMouseCmdGetDeviceId = 0xF2,
    kMouseCmdSetSampleRate = 0xF3,
    kMouseCmdSetResolution = 0xE8,

    kControllerTimeout = 100000,
};

static bool s_ready = false;
static int s_x = 0;
static int s_y = 0;
static int s_max_x = 1023;
static int s_max_y = 767;
static bool s_left = false;
static bool s_right = false;
static bool s_middle = false;
static uint8_t s_motion_level = 2;

static uint8_t s_packet[4];
static uint8_t s_packet_index = 0;
static uint8_t s_packet_size = 3;

static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
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

static bool wait_input_clear(void) {
    for (uint32_t i = 0; i < kControllerTimeout; ++i) {
        if ((inb(kPs2StatusPort) & kPs2StatusInputFull) == 0) {
            return true;
        }
    }
    return false;
}

static bool read_data(uint8_t* out, bool aux_only) {
    if (out == NULL) {
        return false;
    }

    for (uint32_t i = 0; i < kControllerTimeout; ++i) {
        const uint8_t status = inb(kPs2StatusPort);
        if ((status & kPs2StatusOutputFull) == 0) {
            continue;
        }

        const uint8_t data = inb(kPs2DataPort);
        if (!aux_only || (status & kPs2StatusAuxData) != 0) {
            *out = data;
            return true;
        }
    }
    return false;
}

static void drain_output(void) {
    for (uint32_t i = 0; i < 128; ++i) {
        if ((inb(kPs2StatusPort) & kPs2StatusOutputFull) == 0) {
            return;
        }
        (void)inb(kPs2DataPort);
    }
}

static bool mouse_write(uint8_t value) {
    if (!wait_input_clear()) {
        return false;
    }
    outb(kPs2StatusPort, kPs2CmdWriteMouse);

    if (!wait_input_clear()) {
        return false;
    }
    outb(kPs2DataPort, value);
    return true;
}

static bool mouse_read(uint8_t* out) {
    return read_data(out, true);
}

static bool mouse_send_cmd(uint8_t cmd) {
    for (int tries = 0; tries < 3; ++tries) {
        if (!mouse_write(cmd)) {
            return false;
        }

        uint8_t resp = 0;
        if (!mouse_read(&resp)) {
            return false;
        }
        if (resp == kMouseAck) {
            return true;
        }
        if (resp != kMouseResend) {
            return false;
        }
    }
    return false;
}

static bool mouse_send_cmd_arg(uint8_t cmd, uint8_t arg) {
    if (!mouse_send_cmd(cmd)) {
        return false;
    }
    return mouse_send_cmd(arg);
}

static bool ps2_read_config(uint8_t* config) {
    if (config == NULL) {
        return false;
    }
    if (!wait_input_clear()) {
        return false;
    }
    outb(kPs2StatusPort, kPs2CmdReadConfig);
    return read_data(config, false);
}

static bool ps2_write_config(uint8_t config) {
    if (!wait_input_clear()) {
        return false;
    }
    outb(kPs2StatusPort, kPs2CmdWriteConfig);
    if (!wait_input_clear()) {
        return false;
    }
    outb(kPs2DataPort, config);
    return true;
}

static void mouse_try_enable_wheel(void) {
    if (!mouse_send_cmd_arg(kMouseCmdSetSampleRate, 200)) {
        return;
    }
    if (!mouse_send_cmd_arg(kMouseCmdSetSampleRate, 100)) {
        return;
    }
    if (!mouse_send_cmd_arg(kMouseCmdSetSampleRate, 80)) {
        return;
    }
    if (!mouse_send_cmd(kMouseCmdGetDeviceId)) {
        return;
    }

    uint8_t id = 0;
    if (!mouse_read(&id)) {
        return;
    }
    if (id == 0x03U || id == 0x04U) {
        s_packet_size = 4;
    }
}

static void mouse_capture_state(mouse_state* out, int8_t wheel_delta) {
    out->x = s_x;
    out->y = s_y;
    out->left = s_left;
    out->right = s_right;
    out->middle = s_middle;
    out->wheel_delta = wheel_delta;
}

static int apply_sensitivity(int delta) {
    return delta * (int)s_motion_level;
}

void mouse_set_bounds(uint32_t screen_w, uint32_t screen_h) {
    if (screen_w > 0) {
        s_max_x = (int)(screen_w - 1U);
    }
    if (screen_h > 0) {
        s_max_y = (int)(screen_h - 1U);
    }

    s_x = clamp_i32(s_x, 0, s_max_x);
    s_y = clamp_i32(s_y, 0, s_max_y);
}

void mouse_set_sensitivity(uint8_t level) {
    if (level < 1U) {
        level = 1U;
    } else if (level > 4U) {
        level = 4U;
    }
    s_motion_level = level;
}

void mouse_init(uint32_t screen_w, uint32_t screen_h) {
    s_ready = false;
    s_packet_index = 0;
    s_packet_size = 3;
    s_left = false;
    s_right = false;
    s_middle = false;
    mouse_set_sensitivity(2);
    mouse_set_bounds(screen_w, screen_h);

    s_x = s_max_x / 2;
    s_y = s_max_y / 2;

    drain_output();

    if (!wait_input_clear()) {
        return;
    }
    outb(kPs2StatusPort, kPs2CmdEnableAuxPort);

    uint8_t config = 0;
    if (!ps2_read_config(&config)) {
        return;
    }
    config |= kPs2CfgIrq12Enable;
    config &= (uint8_t)~kPs2CfgDisableAuxClock;
    if (!ps2_write_config(config)) {
        return;
    }

    if (!mouse_send_cmd(kMouseCmdResetDefaults)) {
        return;
    }

    (void)mouse_send_cmd_arg(kMouseCmdSetResolution, 2);
    (void)mouse_send_cmd_arg(kMouseCmdSetSampleRate, 100);
    mouse_try_enable_wheel();

    if (!mouse_send_cmd(kMouseCmdEnableStreaming)) {
        return;
    }

    drain_output();
    s_ready = true;
}

bool mouse_ready(void) {
    return s_ready;
}

bool mouse_get_state(mouse_state* out) {
    if (!s_ready || out == NULL) {
        return false;
    }
    mouse_capture_state(out, 0);
    return true;
}

bool mouse_poll(mouse_state* out) {
    if (!s_ready || out == NULL) {
        return false;
    }

    bool updated = false;
    int8_t wheel_accum = 0;

    for (;;) {
        const uint8_t status = inb(kPs2StatusPort);
        if ((status & kPs2StatusOutputFull) == 0) {
            break;
        }
        if ((status & kPs2StatusAuxData) == 0) {
            break;
        }

        const uint8_t data = inb(kPs2DataPort);

        if (s_packet_index == 0 && (data & 0x08U) == 0) {
            continue;
        }

        s_packet[s_packet_index++] = data;
        if (s_packet_index < s_packet_size) {
            continue;
        }
        s_packet_index = 0;

        if ((s_packet[0] & 0xC0U) != 0) {
            continue;
        }

        int dx = (int)((int8_t)s_packet[1]);
        int dy = -(int)((int8_t)s_packet[2]);
        int wheel = 0;
        if (s_packet_size == 4) {
            const uint8_t raw = (uint8_t)(s_packet[3] & 0x0FU);
            wheel = (raw & 0x08U) != 0 ? (int)((int8_t)(raw | 0xF0U)) : (int)raw;
        }

        dx = apply_sensitivity(dx);
        dy = apply_sensitivity(dy);

        const bool new_left = (s_packet[0] & 0x01U) != 0;
        const bool new_right = (s_packet[0] & 0x02U) != 0;
        const bool new_middle = (s_packet[0] & 0x04U) != 0;

        const int nx = clamp_i32(s_x + dx, 0, s_max_x);
        const int ny = clamp_i32(s_y + dy, 0, s_max_y);

        if (nx != s_x || ny != s_y ||
            new_left != s_left || new_right != s_right || new_middle != s_middle ||
            wheel != 0) {
            s_x = nx;
            s_y = ny;
            s_left = new_left;
            s_right = new_right;
            s_middle = new_middle;

            int total_wheel = (int)wheel_accum + wheel;
            if (total_wheel > 127) {
                total_wheel = 127;
            } else if (total_wheel < -128) {
                total_wheel = -128;
            }
            wheel_accum = (int8_t)total_wheel;
            updated = true;
        }
    }

    if (!updated) {
        return false;
    }

    mouse_capture_state(out, wheel_accum);
    return true;
}
