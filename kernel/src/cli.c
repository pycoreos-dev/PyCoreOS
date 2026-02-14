#include "kernel/cli.h"

#include "drivers/ata.h"
#include "drivers/mouse.h"
#include "gui/desktop.h"
#include "kernel/display.h"
#include "kernel/filesystem.h"
#include "kernel/fs_persist.h"
#include "kernel/net_stack.h"
#include "kernel/release.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static size_t cstr_len(const char* s) {
    size_t n = 0;
    while (s[n] != '\0') {
        ++n;
    }
    return n;
}

static bool str_eq(const char* a, const char* b) {
    size_t i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) {
            return false;
        }
        ++i;
    }
    return a[i] == b[i];
}

static bool starts_with(const char* s, const char* prefix) {
    size_t i = 0;
    while (prefix[i]) {
        if (s[i] != prefix[i]) {
            return false;
        }
        ++i;
    }
    return true;
}

static const char* skip_ws(const char* s) {
    while (*s == ' ') {
        ++s;
    }
    return s;
}

static bool parse_arg(const char** inout, char* out, size_t out_cap) {
    const char* s = skip_ws(*inout);
    if (*s == '\0') {
        return false;
    }

    size_t i = 0;
    while (*s && *s != ' ' && i + 1 < out_cap) {
        out[i++] = *s++;
    }
    out[i] = '\0';

    while (*s && *s != ' ') {
        ++s;
    }
    *inout = s;
    return true;
}

static bool parse_u32(const char* s, uint32_t* out) {
    if (s == NULL || *s == '\0' || out == NULL) {
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

static bool parse_ipv4(const char* s, uint32_t* out_ipv4_be) {
    if (s == NULL || out_ipv4_be == NULL) {
        return false;
    }

    uint32_t octet = 0;
    uint32_t parts[4] = {0, 0, 0, 0};
    uint32_t part_idx = 0;
    bool have_digit = false;

    for (size_t i = 0;; ++i) {
        const char c = s[i];
        if (c >= '0' && c <= '9') {
            have_digit = true;
            octet = octet * 10U + (uint32_t)(c - '0');
            if (octet > 255U) {
                return false;
            }
            continue;
        }

        if (c == '.' || c == '\0') {
            if (!have_digit || part_idx >= 4U) {
                return false;
            }
            parts[part_idx++] = octet;
            octet = 0;
            have_digit = false;
            if (c == '\0') {
                break;
            }
            continue;
        }

        return false;
    }

    if (part_idx != 4U) {
        return false;
    }

    *out_ipv4_be = (parts[0] << 24U) | (parts[1] << 16U) | (parts[2] << 8U) | parts[3];
    return true;
}

static void format_seconds_hms(uint32_t seconds, char* out, size_t cap) {
    if (cap < 9U) {
        if (cap > 0U) {
            out[0] = '\0';
        }
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

static void buf_append_char(char* out, size_t cap, size_t* idx, char c);
static void buf_append_str(char* out, size_t cap, size_t* idx, const char* text);
static void buf_append_u32(char* out, size_t cap, size_t* idx, uint32_t value);

static bool write_beta_report_file(void) {
    char report[640];
    size_t idx = 0;
    report[0] = '\0';

    buf_append_str(report, sizeof(report), &idx, "PyCoreOS Beta Report\n");
    buf_append_str(report, sizeof(report), &idx, "version=");
    buf_append_str(report, sizeof(report), &idx, pycoreos_version());
    buf_append_char(report, sizeof(report), &idx, '\n');
    buf_append_str(report, sizeof(report), &idx, "channel=");
    buf_append_str(report, sizeof(report), &idx, pycoreos_channel());
    buf_append_char(report, sizeof(report), &idx, '\n');
    buf_append_str(report, sizeof(report), &idx, "codename=");
    buf_append_str(report, sizeof(report), &idx, pycoreos_codename());
    buf_append_char(report, sizeof(report), &idx, '\n');
    buf_append_str(report, sizeof(report), &idx, "build=");
    buf_append_str(report, sizeof(report), &idx, pycoreos_build_stamp());
    buf_append_char(report, sizeof(report), &idx, '\n');

    buf_append_str(report, sizeof(report), &idx, "uptime_s=");
    buf_append_u32(report, sizeof(report), &idx, desktop_uptime_seconds());
    buf_append_char(report, sizeof(report), &idx, '\n');

    buf_append_str(report, sizeof(report), &idx, "display=");
    buf_append_u32(report, sizeof(report), &idx, display_width());
    buf_append_char(report, sizeof(report), &idx, 'x');
    buf_append_u32(report, sizeof(report), &idx, display_height());
    buf_append_char(report, sizeof(report), &idx, 'x');
    buf_append_u32(report, sizeof(report), &idx, display_bpp());
    buf_append_str(report, sizeof(report), &idx, " pitch=");
    buf_append_u32(report, sizeof(report), &idx, display_pitch());
    buf_append_char(report, sizeof(report), &idx, '\n');

    buf_append_str(report, sizeof(report), &idx, "fs_files=");
    buf_append_u32(report, sizeof(report), &idx, (uint32_t)fs_count());
    buf_append_str(report, sizeof(report), &idx, " ram_used=");
    buf_append_u32(report, sizeof(report), &idx, (uint32_t)fs_ramdisk_used());
    buf_append_str(report, sizeof(report), &idx, " ram_cap=");
    buf_append_u32(report, sizeof(report), &idx, (uint32_t)fs_ramdisk_capacity());
    buf_append_char(report, sizeof(report), &idx, '\n');

    buf_append_str(report, sizeof(report), &idx, "persist=");
    buf_append_char(report, sizeof(report), &idx, fs_persist_available() ? '1' : '0');
    buf_append_char(report, sizeof(report), &idx, '\n');

    return fs_write("beta_report.txt", report);
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

static void buf_append_i32(char* out, size_t cap, size_t* idx, int32_t value) {
    if (value < 0) {
        buf_append_char(out, cap, idx, '-');
        value = -value;
    }
    buf_append_u32(out, cap, idx, (uint32_t)value);
}

enum {
    kHistoryMax = 40,
    kHistoryLineMax = 80,
    kPreviewBufMax = 1024,
};

static char s_history[kHistoryMax][kHistoryLineMax];
static size_t s_history_count = 0;

static void copy_cstr(char* dst, size_t cap, const char* src) {
    if (dst == NULL || cap == 0 || src == NULL) {
        return;
    }
    size_t i = 0;
    while (src[i] != '\0' && i + 1 < cap) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static bool parse_i32(const char* s, int32_t* out) {
    if (s == NULL || out == NULL || *s == '\0') {
        return false;
    }

    bool neg = false;
    size_t i = 0;
    if (s[0] == '-') {
        neg = true;
        i = 1;
    }
    if (s[i] == '\0') {
        return false;
    }

    int32_t v = 0;
    for (; s[i] != '\0'; ++i) {
        if (s[i] < '0' || s[i] > '9') {
            return false;
        }
        v = v * 10 + (int32_t)(s[i] - '0');
    }

    *out = neg ? -v : v;
    return true;
}

static bool is_space_char(char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

static bool cstr_contains(const char* text, const char* needle) {
    if (text == NULL || needle == NULL || needle[0] == '\0') {
        return false;
    }

    for (size_t i = 0; text[i] != '\0'; ++i) {
        size_t j = 0;
        while (needle[j] != '\0' && text[i + j] != '\0' && text[i + j] == needle[j]) {
            ++j;
        }
        if (needle[j] == '\0') {
            return true;
        }
    }
    return false;
}

static void history_push(const char* cmd) {
    if (cmd == NULL || cmd[0] == '\0') {
        return;
    }

    if (s_history_count < kHistoryMax) {
        copy_cstr(s_history[s_history_count], sizeof(s_history[s_history_count]), cmd);
        ++s_history_count;
        return;
    }

    for (size_t i = 1; i < kHistoryMax; ++i) {
        copy_cstr(s_history[i - 1], sizeof(s_history[i - 1]), s_history[i]);
    }
    copy_cstr(s_history[kHistoryMax - 1], sizeof(s_history[kHistoryMax - 1]), cmd);
}

static void history_print(void) {
    if (s_history_count == 0) {
        desktop_append_log("(history empty)");
        return;
    }

    for (size_t i = 0; i < s_history_count; ++i) {
        char line[116];
        size_t idx = 0;
        line[0] = '\0';
        buf_append_u32(line, sizeof(line), &idx, (uint32_t)(i + 1));
        buf_append_str(line, sizeof(line), &idx, ": ");
        buf_append_str(line, sizeof(line), &idx, s_history[i]);
        desktop_append_log(line);
    }
}

static void log_multiline_text(const char* text) {
    if (text == NULL) {
        return;
    }

    size_t i = 0;
    while (text[i] != '\0') {
        char line[160];
        size_t out = 0;
        while (text[i] != '\0' && text[i] != '\n' && out + 1 < sizeof(line)) {
            line[out++] = text[i++];
        }
        line[out] = '\0';
        if (text[i] == '\n') {
            ++i;
        }
        if (out > 0) {
            desktop_append_log(line);
        }
    }
}

static bool append_line_to_file(const char* filename, const char* line) {
    if (filename == NULL || line == NULL) {
        return false;
    }

    char data[kPreviewBufMax];
    if (!fs_read(filename, data, sizeof(data))) {
        data[0] = '\0';
    }

    size_t len = cstr_len(data);
    const size_t add = cstr_len(line);
    if (len + add + 2 >= sizeof(data)) {
        return false;
    }

    if (len > 0 && data[len - 1] != '\n') {
        data[len++] = '\n';
    }
    for (size_t i = 0; i < add; ++i) {
        data[len++] = line[i];
    }
    data[len++] = '\n';
    data[len] = '\0';
    return fs_write(filename, data);
}

static uint32_t clamp_u32(uint32_t v, uint32_t lo, uint32_t hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

void cli_init(void) {
    desktop_append_log("Commands: help about version beta uname whoami hostname date time");
    desktop_append_log("ls cat touch write append rm cp mv stat find head tail grep wc");
    desktop_append_log("clip todo journal apps open resmode calc history");
    desktop_append_log("display mouse fsinfo meminfo netinfo sysinfo savefs loadfs ping");
    desktop_append_log("betareport clear doom");
    desktop_append_log("power: sleep logout/signout/logoff restart shutdown");
}

cli_action cli_execute(const char* line) {
    const char* p = skip_ws(line);
    if (*p == '\0') {
        return CLI_ACTION_NONE;
    }

    if (!str_eq(p, "history")) {
        history_push(p);
    }

    if (str_eq(p, "help")) {
        desktop_append_log("core: help/about/version/beta/uname/whoami/hostname/date/time/history");
        desktop_append_log("files: ls/cat/touch/write/append/rm/cp/mv/stat/find/head/tail/grep/wc");
        desktop_append_log("workspace: clip/todo/journal/apps/open/resmode/calc");
        desktop_append_log("system: display/mouse/fsinfo/meminfo/netinfo/sysinfo");
        desktop_append_log("persist: savefs/loadfs/sync/save betareport ping clear doom");
        desktop_append_log("power: sleep/logout/signout/logoff/restart/shutdown");
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "about")) {
        desktop_append_log("PyCoreOS: Win2000-style desktop, shell, drivers, and app framework.");
        desktop_append_log("Version: " PYCOREOS_VERSION " (" PYCOREOS_CHANNEL ")");
        desktop_append_log("Lead OSDev: Johan Joseph");
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "version")) {
        char line[160];
        size_t idx = 0;
        line[0] = '\0';
        buf_append_str(line, sizeof(line), &idx, "PyCoreOS ");
        buf_append_str(line, sizeof(line), &idx, pycoreos_version());
        buf_append_str(line, sizeof(line), &idx, " (");
        buf_append_str(line, sizeof(line), &idx, pycoreos_channel());
        buf_append_str(line, sizeof(line), &idx, ") ");
        buf_append_str(line, sizeof(line), &idx, pycoreos_codename());
        desktop_append_log(line);

        idx = 0;
        line[0] = '\0';
        buf_append_str(line, sizeof(line), &idx, "build ");
        buf_append_str(line, sizeof(line), &idx, pycoreos_build_stamp());
        desktop_append_log(line);
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "uname")) {
        desktop_append_log("PyCoreOS 32-bit educational kernel");
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "whoami")) {
        desktop_append_log(desktop_current_user());
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "hostname")) {
        desktop_append_log("pycoreos");
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "date")) {
        char msg[128];
        size_t idx = 0;
        msg[0] = '\0';
        buf_append_str(msg, sizeof(msg), &idx, "build ");
        buf_append_str(msg, sizeof(msg), &idx, pycoreos_build_stamp());
        buf_append_str(msg, sizeof(msg), &idx, " uptime=");
        char hms[16];
        format_seconds_hms(desktop_uptime_seconds(), hms, sizeof(hms));
        buf_append_str(msg, sizeof(msg), &idx, hms);
        desktop_append_log(msg);
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "time")) {
        char msg[64];
        size_t idx = 0;
        msg[0] = '\0';
        buf_append_str(msg, sizeof(msg), &idx, "session ");
        char hms[16];
        format_seconds_hms(desktop_uptime_seconds(), hms, sizeof(hms));
        buf_append_str(msg, sizeof(msg), &idx, hms);
        desktop_append_log(msg);
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "history")) {
        history_print();
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "beta")) {
        desktop_append_log("Public beta build: expect bugs and missing features.");
        desktop_append_log("Use 'betareport' to write diagnostics to beta_report.txt.");
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "pwd")) {
        desktop_append_log("/");
        return CLI_ACTION_NONE;
    }

    if (starts_with(p, "echo")) {
        p += 4;
        p = skip_ws(p);
        if (*p == '\0') {
            desktop_append_log("(empty)");
        } else {
            desktop_append_log(p);
        }
        return CLI_ACTION_NONE;
    }

    if (starts_with(p, "find ")) {
        p += 5;
        char needle[48];
        if (!parse_arg(&p, needle, sizeof(needle))) {
            desktop_append_log("usage: find <name-fragment>");
            return CLI_ACTION_NONE;
        }

        bool found = false;
        for (size_t i = 0; i < fs_count(); ++i) {
            char name[56];
            if (!fs_name_at(i, name, sizeof(name))) {
                continue;
            }
            if (!cstr_contains(name, needle)) {
                continue;
            }
            desktop_append_log(name);
            found = true;
        }
        if (!found) {
            desktop_append_log("find: no matches");
        }
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "ls")) {
        size_t n = fs_count();
        if (n == 0) {
            desktop_append_log("(filesystem empty)");
            return CLI_ACTION_NONE;
        }
        for (size_t i = 0; i < n; ++i) {
            char name[32];
            if (fs_name_at(i, name, sizeof(name))) {
                desktop_append_log(name);
            }
        }
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "fsinfo")) {
        char msg[128];
        size_t idx = 0;
        msg[0] = '\0';
        buf_append_str(msg, sizeof(msg), &idx, "files=");
        buf_append_u32(msg, sizeof(msg), &idx, (uint32_t)fs_count());
        buf_append_str(msg, sizeof(msg), &idx, " ram_used=");
        buf_append_u32(msg, sizeof(msg), &idx, (uint32_t)fs_ramdisk_used());
        buf_append_str(msg, sizeof(msg), &idx, " ram_cap=");
        buf_append_u32(msg, sizeof(msg), &idx, (uint32_t)fs_ramdisk_capacity());
        desktop_append_log(msg);
        desktop_append_log(fs_persist_available() ? "persist: available" : "persist: unavailable");
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "meminfo")) {
        const uint32_t used = (uint32_t)fs_ramdisk_used();
        const uint32_t cap = (uint32_t)fs_ramdisk_capacity();
        uint32_t pct = 0;
        if (cap > 0U) {
            pct = (used * 100U) / cap;
        }
        pct = clamp_u32(pct, 0U, 100U);

        char msg[96];
        size_t idx = 0;
        msg[0] = '\0';
        buf_append_str(msg, sizeof(msg), &idx, "ramdisk used=");
        buf_append_u32(msg, sizeof(msg), &idx, used);
        buf_append_str(msg, sizeof(msg), &idx, " cap=");
        buf_append_u32(msg, sizeof(msg), &idx, cap);
        buf_append_str(msg, sizeof(msg), &idx, " (");
        buf_append_u32(msg, sizeof(msg), &idx, pct);
        buf_append_str(msg, sizeof(msg), &idx, "%)");
        desktop_append_log(msg);
        return CLI_ACTION_NONE;
    }

    if (starts_with(p, "cat ")) {
        p += 4;
        char name[32];
        if (!parse_arg(&p, name, sizeof(name))) {
            desktop_append_log("usage: cat <file>");
            return CLI_ACTION_NONE;
        }
        char data[280];
        if (!fs_read(name, data, sizeof(data))) {
            desktop_append_log("cat: file not found");
            return CLI_ACTION_NONE;
        }
        desktop_append_log(data);
        return CLI_ACTION_NONE;
    }

    if (starts_with(p, "head ")) {
        p += 5;
        char name[32];
        if (!parse_arg(&p, name, sizeof(name))) {
            desktop_append_log("usage: head <file> [lines]");
            return CLI_ACTION_NONE;
        }

        uint32_t lines = 10U;
        char count_arg[12];
        if (parse_arg(&p, count_arg, sizeof(count_arg))) {
            if (!parse_u32(count_arg, &lines) || lines == 0U) {
                desktop_append_log("head: invalid line count");
                return CLI_ACTION_NONE;
            }
        }
        lines = clamp_u32(lines, 1U, 40U);

        char data[kPreviewBufMax];
        if (!fs_read(name, data, sizeof(data))) {
            desktop_append_log("head: file not found");
            return CLI_ACTION_NONE;
        }

        size_t i = 0;
        uint32_t shown = 0;
        while (data[i] != '\0' && shown < lines) {
            char row[160];
            size_t out = 0;
            while (data[i] != '\0' && data[i] != '\n' && out + 1 < sizeof(row)) {
                row[out++] = data[i++];
            }
            row[out] = '\0';
            if (data[i] == '\n') {
                ++i;
            }
            if (out > 0) {
                desktop_append_log(row);
            }
            ++shown;
        }
        if (shown == 0U) {
            desktop_append_log("(empty file)");
        }
        return CLI_ACTION_NONE;
    }

    if (starts_with(p, "tail ")) {
        p += 5;
        char name[32];
        if (!parse_arg(&p, name, sizeof(name))) {
            desktop_append_log("usage: tail <file> [lines]");
            return CLI_ACTION_NONE;
        }

        uint32_t lines = 10U;
        char count_arg[12];
        if (parse_arg(&p, count_arg, sizeof(count_arg))) {
            if (!parse_u32(count_arg, &lines) || lines == 0U) {
                desktop_append_log("tail: invalid line count");
                return CLI_ACTION_NONE;
            }
        }
        lines = clamp_u32(lines, 1U, 40U);

        char data[kPreviewBufMax];
        if (!fs_read(name, data, sizeof(data))) {
            desktop_append_log("tail: file not found");
            return CLI_ACTION_NONE;
        }
        if (data[0] == '\0') {
            desktop_append_log("(empty file)");
            return CLI_ACTION_NONE;
        }

        const size_t len = cstr_len(data);
        size_t start = 0;
        uint32_t seen = 0;
        for (size_t i = len; i > 0; --i) {
            if (data[i - 1] == '\n') {
                ++seen;
                if (seen > lines) {
                    start = i;
                    break;
                }
            }
        }
        log_multiline_text(data + start);
        return CLI_ACTION_NONE;
    }

    if (starts_with(p, "touch ")) {
        p += 6;
        char name[32];
        if (!parse_arg(&p, name, sizeof(name))) {
            desktop_append_log("usage: touch <file>");
            return CLI_ACTION_NONE;
        }
        if (fs_touch(name)) {
            desktop_append_log("touch: ok");
        } else {
            desktop_append_log("touch: failed");
        }
        return CLI_ACTION_NONE;
    }

    if (starts_with(p, "rm ")) {
        p += 3;
        char name[32];
        if (!parse_arg(&p, name, sizeof(name))) {
            desktop_append_log("usage: rm <file>");
            return CLI_ACTION_NONE;
        }
        if (fs_remove(name)) {
            desktop_append_log("rm: removed");
        } else {
            desktop_append_log("rm: file not found");
        }
        return CLI_ACTION_NONE;
    }

    if (starts_with(p, "write ")) {
        p += 6;
        char name[32];
        if (!parse_arg(&p, name, sizeof(name))) {
            desktop_append_log("usage: write <file> <content>");
            return CLI_ACTION_NONE;
        }
        p = skip_ws(p);
        if (*p == '\0') {
            desktop_append_log("usage: write <file> <content>");
            return CLI_ACTION_NONE;
        }
        if (fs_write(name, p)) {
            desktop_append_log("write: saved");
        } else {
            desktop_append_log("write: failed");
        }
        return CLI_ACTION_NONE;
    }

    if (starts_with(p, "append ")) {
        p += 7;
        char name[32];
        if (!parse_arg(&p, name, sizeof(name))) {
            desktop_append_log("usage: append <file> <content>");
            return CLI_ACTION_NONE;
        }
        p = skip_ws(p);
        if (*p == '\0') {
            desktop_append_log("usage: append <file> <content>");
            return CLI_ACTION_NONE;
        }

        char data[280];
        if (!fs_read(name, data, sizeof(data))) {
            desktop_append_log("append: file not found");
            return CLI_ACTION_NONE;
        }

        const size_t base_len = cstr_len(data);
        const size_t add_len = cstr_len(p);
        if (base_len + add_len + 1 >= sizeof(data)) {
            desktop_append_log("append: file too large");
            return CLI_ACTION_NONE;
        }

        size_t i = base_len;
        size_t j = 0;
        while (p[j] != '\0' && i + 1 < sizeof(data)) {
            data[i++] = p[j++];
        }
        data[i] = '\0';

        if (fs_write(name, data)) {
            desktop_append_log("append: done");
        } else {
            desktop_append_log("append: failed");
        }
        return CLI_ACTION_NONE;
    }

    if (starts_with(p, "cp ")) {
        p += 3;
        char src[32];
        char dst[32];
        if (!parse_arg(&p, src, sizeof(src)) || !parse_arg(&p, dst, sizeof(dst))) {
            desktop_append_log("usage: cp <src> <dst>");
            return CLI_ACTION_NONE;
        }

        char data[280];
        if (!fs_read(src, data, sizeof(data))) {
            desktop_append_log("cp: source not found");
            return CLI_ACTION_NONE;
        }

        if (fs_write(dst, data)) {
            desktop_append_log("cp: copied");
        } else {
            desktop_append_log("cp: failed");
        }
        return CLI_ACTION_NONE;
    }

    if (starts_with(p, "mv ")) {
        p += 3;
        char src[32];
        char dst[32];
        if (!parse_arg(&p, src, sizeof(src)) || !parse_arg(&p, dst, sizeof(dst))) {
            desktop_append_log("usage: mv <src> <dst>");
            return CLI_ACTION_NONE;
        }

        if (str_eq(src, dst)) {
            desktop_append_log("mv: source and destination are identical");
            return CLI_ACTION_NONE;
        }

        char data[280];
        if (!fs_read(src, data, sizeof(data))) {
            desktop_append_log("mv: source not found");
            return CLI_ACTION_NONE;
        }

        if (!fs_write(dst, data)) {
            desktop_append_log("mv: write failed");
            return CLI_ACTION_NONE;
        }
        if (!fs_remove(src)) {
            desktop_append_log("mv: remove failed");
            return CLI_ACTION_NONE;
        }

        desktop_append_log("mv: moved");
        return CLI_ACTION_NONE;
    }

    if (starts_with(p, "stat ")) {
        p += 5;
        char name[32];
        if (!parse_arg(&p, name, sizeof(name))) {
            desktop_append_log("usage: stat <file>");
            return CLI_ACTION_NONE;
        }

        char data[280];
        if (!fs_read(name, data, sizeof(data))) {
            desktop_append_log("stat: file not found");
            return CLI_ACTION_NONE;
        }

        char msg[96];
        size_t idx = 0;
        msg[0] = '\0';
        buf_append_str(msg, sizeof(msg), &idx, "stat ");
        buf_append_str(msg, sizeof(msg), &idx, name);
        buf_append_str(msg, sizeof(msg), &idx, " size=");
        buf_append_u32(msg, sizeof(msg), &idx, (uint32_t)cstr_len(data));
        buf_append_str(msg, sizeof(msg), &idx, " bytes");
        desktop_append_log(msg);
        return CLI_ACTION_NONE;
    }

    if (starts_with(p, "grep ")) {
        p += 5;
        char needle[48];
        char name[32];
        if (!parse_arg(&p, needle, sizeof(needle)) || !parse_arg(&p, name, sizeof(name))) {
            desktop_append_log("usage: grep <needle> <file>");
            return CLI_ACTION_NONE;
        }

        char data[kPreviewBufMax];
        if (!fs_read(name, data, sizeof(data))) {
            desktop_append_log("grep: file not found");
            return CLI_ACTION_NONE;
        }

        bool matched = false;
        size_t i = 0;
        while (data[i] != '\0') {
            char row[160];
            size_t out = 0;
            while (data[i] != '\0' && data[i] != '\n' && out + 1 < sizeof(row)) {
                row[out++] = data[i++];
            }
            row[out] = '\0';
            if (data[i] == '\n') {
                ++i;
            }
            if (out > 0 && cstr_contains(row, needle)) {
                desktop_append_log(row);
                matched = true;
            }
        }
        if (!matched) {
            desktop_append_log("grep: no matches");
        }
        return CLI_ACTION_NONE;
    }

    if (starts_with(p, "wc ")) {
        p += 3;
        char name[32];
        if (!parse_arg(&p, name, sizeof(name))) {
            desktop_append_log("usage: wc <file>");
            return CLI_ACTION_NONE;
        }

        char data[kPreviewBufMax];
        if (!fs_read(name, data, sizeof(data))) {
            desktop_append_log("wc: file not found");
            return CLI_ACTION_NONE;
        }

        const size_t bytes = cstr_len(data);
        uint32_t lines = 0;
        uint32_t words = 0;
        bool in_word = false;
        for (size_t i = 0; i < bytes; ++i) {
            const char c = data[i];
            if (c == '\n') {
                ++lines;
            }
            if (is_space_char(c)) {
                in_word = false;
            } else if (!in_word) {
                in_word = true;
                ++words;
            }
        }
        if (bytes > 0 && data[bytes - 1] != '\n') {
            ++lines;
        }

        char msg[112];
        size_t idx = 0;
        msg[0] = '\0';
        buf_append_str(msg, sizeof(msg), &idx, "lines=");
        buf_append_u32(msg, sizeof(msg), &idx, lines);
        buf_append_str(msg, sizeof(msg), &idx, " words=");
        buf_append_u32(msg, sizeof(msg), &idx, words);
        buf_append_str(msg, sizeof(msg), &idx, " bytes=");
        buf_append_u32(msg, sizeof(msg), &idx, (uint32_t)bytes);
        desktop_append_log(msg);
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "clip")) {
        char data[kPreviewBufMax];
        if (!fs_read("clipboard.txt", data, sizeof(data)) || data[0] == '\0') {
            desktop_append_log("(clipboard empty)");
        } else {
            log_multiline_text(data);
        }
        return CLI_ACTION_NONE;
    }

    if (starts_with(p, "clip set ")) {
        p += 9;
        p = skip_ws(p);
        if (*p == '\0') {
            desktop_append_log("usage: clip set <text>");
            return CLI_ACTION_NONE;
        }
        if (fs_write("clipboard.txt", p)) {
            desktop_append_log("clip: saved");
        } else {
            desktop_append_log("clip: failed");
        }
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "clip clear")) {
        (void)fs_write("clipboard.txt", "");
        desktop_append_log("clip: cleared");
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "todo")) {
        char data[kPreviewBufMax];
        if (!fs_read("todo.txt", data, sizeof(data)) || data[0] == '\0') {
            desktop_append_log("(todo empty)");
        } else {
            log_multiline_text(data);
        }
        return CLI_ACTION_NONE;
    }

    if (starts_with(p, "todo add ")) {
        p += 9;
        p = skip_ws(p);
        if (*p == '\0') {
            desktop_append_log("usage: todo add <text>");
            return CLI_ACTION_NONE;
        }
        if (append_line_to_file("todo.txt", p)) {
            desktop_append_log("todo: added");
        } else {
            desktop_append_log("todo: failed (file full)");
        }
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "todo clear")) {
        (void)fs_write("todo.txt", "");
        desktop_append_log("todo: cleared");
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "journal")) {
        char data[kPreviewBufMax];
        if (!fs_read("journal.txt", data, sizeof(data)) || data[0] == '\0') {
            desktop_append_log("(journal empty)");
        } else {
            log_multiline_text(data);
        }
        return CLI_ACTION_NONE;
    }

    if (starts_with(p, "journal add ")) {
        p += 12;
        p = skip_ws(p);
        if (*p == '\0') {
            desktop_append_log("usage: journal add <text>");
            return CLI_ACTION_NONE;
        }

        char entry[160];
        size_t idx = 0;
        entry[0] = '\0';
        buf_append_char(entry, sizeof(entry), &idx, '[');
        buf_append_u32(entry, sizeof(entry), &idx, desktop_uptime_seconds());
        buf_append_str(entry, sizeof(entry), &idx, "s] ");
        buf_append_str(entry, sizeof(entry), &idx, p);
        if (append_line_to_file("journal.txt", entry)) {
            desktop_append_log("journal: entry saved");
        } else {
            desktop_append_log("journal: failed (file full)");
        }
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "journal clear")) {
        (void)fs_write("journal.txt", "");
        desktop_append_log("journal: cleared");
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "display")) {
        if (!display_ready()) {
            desktop_append_log("display: text fallback");
            return CLI_ACTION_NONE;
        }

        char msg[96];
        size_t idx = 0;
        msg[0] = '\0';
        buf_append_str(msg, sizeof(msg), &idx, "display ");
        buf_append_u32(msg, sizeof(msg), &idx, display_width());
        buf_append_char(msg, sizeof(msg), &idx, 'x');
        buf_append_u32(msg, sizeof(msg), &idx, display_height());
        buf_append_char(msg, sizeof(msg), &idx, 'x');
        buf_append_u32(msg, sizeof(msg), &idx, display_bpp());
        buf_append_str(msg, sizeof(msg), &idx, " pitch=");
        buf_append_u32(msg, sizeof(msg), &idx, display_pitch());
        desktop_append_log(msg);
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "uptime")) {
        char msg[64];
        size_t idx = 0;
        msg[0] = '\0';
        buf_append_str(msg, sizeof(msg), &idx, "uptime ");
        buf_append_u32(msg, sizeof(msg), &idx, desktop_uptime_seconds());
        buf_append_char(msg, sizeof(msg), &idx, 's');
        desktop_append_log(msg);
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "sleep")) {
        desktop_enter_sleep_mode();
        desktop_append_log("sleep: move mouse or press any key to wake");
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "logout") || str_eq(p, "signout") || str_eq(p, "logoff")) {
        desktop_append_log("logout: returning to sign-in screen");
        desktop_logout_session();
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "restart")) {
        desktop_append_log("restart: rebooting...");
        return CLI_ACTION_RESTART;
    }

    if (str_eq(p, "shutdown")) {
        desktop_append_log("shutdown: halting cpu");
        return CLI_ACTION_SHUTDOWN;
    }

    if (str_eq(p, "netinfo")) {
        desktop_append_log(net_stack_ready() ? "network: ready" : "network: unavailable");
        desktop_append_log("use: ping <a.b.c.d>");
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "sysinfo")) {
        char msg[160];
        size_t idx = 0;
        msg[0] = '\0';
        buf_append_str(msg, sizeof(msg), &idx, "PyCoreOS ");
        buf_append_str(msg, sizeof(msg), &idx, pycoreos_version());
        buf_append_str(msg, sizeof(msg), &idx, " uptime=");
        {
            char hms[16];
            format_seconds_hms(desktop_uptime_seconds(), hms, sizeof(hms));
            buf_append_str(msg, sizeof(msg), &idx, hms);
        }
        desktop_append_log(msg);

        idx = 0;
        msg[0] = '\0';
        buf_append_str(msg, sizeof(msg), &idx, "display ");
        buf_append_u32(msg, sizeof(msg), &idx, display_width());
        buf_append_char(msg, sizeof(msg), &idx, 'x');
        buf_append_u32(msg, sizeof(msg), &idx, display_height());
        buf_append_str(msg, sizeof(msg), &idx, " fs_files=");
        buf_append_u32(msg, sizeof(msg), &idx, (uint32_t)fs_count());
        desktop_append_log(msg);

        idx = 0;
        msg[0] = '\0';
        buf_append_str(msg, sizeof(msg), &idx, "net=");
        buf_append_str(msg, sizeof(msg), &idx, net_stack_ready() ? "ready" : "down");
        desktop_append_log(msg);
        return CLI_ACTION_NONE;
    }

    if (starts_with(p, "calc ")) {
        p += 5;
        char a_arg[20];
        char op_arg[4];
        char b_arg[20];
        if (!parse_arg(&p, a_arg, sizeof(a_arg)) ||
            !parse_arg(&p, op_arg, sizeof(op_arg)) ||
            !parse_arg(&p, b_arg, sizeof(b_arg))) {
            desktop_append_log("usage: calc <a> <+|-|*|/> <b>");
            return CLI_ACTION_NONE;
        }

        int32_t a = 0;
        int32_t b = 0;
        if (!parse_i32(a_arg, &a) || !parse_i32(b_arg, &b)) {
            desktop_append_log("calc: invalid integer");
            return CLI_ACTION_NONE;
        }

        int32_t result = 0;
        const char op = op_arg[0];
        if (op == '+' && op_arg[1] == '\0') {
            result = a + b;
        } else if (op == '-' && op_arg[1] == '\0') {
            result = a - b;
        } else if (op == '*' && op_arg[1] == '\0') {
            result = a * b;
        } else if (op == '/' && op_arg[1] == '\0') {
            if (b == 0) {
                desktop_append_log("calc: division by zero");
                return CLI_ACTION_NONE;
            }
            result = a / b;
        } else {
            desktop_append_log("calc: operator must be + - * /");
            return CLI_ACTION_NONE;
        }

        char msg[96];
        size_t idx = 0;
        msg[0] = '\0';
        buf_append_i32(msg, sizeof(msg), &idx, a);
        buf_append_char(msg, sizeof(msg), &idx, ' ');
        buf_append_char(msg, sizeof(msg), &idx, op);
        buf_append_char(msg, sizeof(msg), &idx, ' ');
        buf_append_i32(msg, sizeof(msg), &idx, b);
        buf_append_str(msg, sizeof(msg), &idx, " = ");
        buf_append_i32(msg, sizeof(msg), &idx, result);
        desktop_append_log(msg);
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "apps")) {
        desktop_append_log("apps core: help files system mouse settings perf notes about credits tips doom editor");
        desktop_append_log("apps extra: calc clock calendar tasks clipboard network storage diagnostics monitor");
        desktop_append_log("apps extra: guide wallpaper shortcuts troubleshoot release roadmap journal todo");
        desktop_append_log("apps extra: packages snapshots launcher");
        desktop_append_log("use: open <app>");
        return CLI_ACTION_NONE;
    }

    if (starts_with(p, "open ")) {
        p += 5;
        char app_name[32];
        if (!parse_arg(&p, app_name, sizeof(app_name))) {
            desktop_append_log("usage: open <app>");
            return CLI_ACTION_NONE;
        }
        if (!desktop_open_app_by_name(app_name)) {
            desktop_append_log("open: unknown app");
            return CLI_ACTION_NONE;
        }
        desktop_append_log("open: app launched");
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "theme")) {
        char msg[64];
        size_t idx = 0;
        msg[0] = '\0';
        buf_append_str(msg, sizeof(msg), &idx, "theme 0 (dark, locked)");
        desktop_append_log(msg);
        return CLI_ACTION_NONE;
    }

    if (starts_with(p, "theme ")) {
        desktop_append_log("theme command disabled (dark mode enforced)");
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "resmode")) {
        desktop_append_log(desktop_resolution_mode() == 0 ? "resmode: native" : "resmode: large");
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "resmode toggle")) {
        desktop_toggle_resolution_mode();
        desktop_append_log(desktop_resolution_mode() == 0 ? "resmode: native" : "resmode: large");
        return CLI_ACTION_NONE;
    }

    if (starts_with(p, "mouse speed ")) {
        p += 12;
        char speed_arg[12];
        if (!parse_arg(&p, speed_arg, sizeof(speed_arg))) {
            desktop_append_log("usage: mouse speed <1-4>");
            return CLI_ACTION_NONE;
        }

        uint32_t speed = 0;
        if (!parse_u32(speed_arg, &speed) || speed < 1U || speed > 4U) {
            desktop_append_log("mouse speed must be 1..4");
            return CLI_ACTION_NONE;
        }

        if (!desktop_set_mouse_speed((uint8_t)speed)) {
            desktop_append_log("mouse speed: failed");
            return CLI_ACTION_NONE;
        }
        desktop_append_log("mouse speed updated");
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "mouse")) {
        if (!mouse_ready()) {
            desktop_append_log("mouse: unavailable");
            return CLI_ACTION_NONE;
        }

        mouse_state ms;
        if (!mouse_get_state(&ms)) {
            desktop_append_log("mouse: state unavailable");
            return CLI_ACTION_NONE;
        }

        char msg[96];
        size_t idx = 0;
        msg[0] = '\0';
        buf_append_str(msg, sizeof(msg), &idx, "mouse x=");
        buf_append_u32(msg, sizeof(msg), &idx, (uint32_t)ms.x);
        buf_append_str(msg, sizeof(msg), &idx, " y=");
        buf_append_u32(msg, sizeof(msg), &idx, (uint32_t)ms.y);
        buf_append_str(msg, sizeof(msg), &idx, " l=");
        buf_append_char(msg, sizeof(msg), &idx, ms.left ? '1' : '0');
        buf_append_str(msg, sizeof(msg), &idx, " r=");
        buf_append_char(msg, sizeof(msg), &idx, ms.right ? '1' : '0');
        buf_append_str(msg, sizeof(msg), &idx, " m=");
        buf_append_char(msg, sizeof(msg), &idx, ms.middle ? '1' : '0');
        if (ms.wheel_delta != 0) {
            buf_append_str(msg, sizeof(msg), &idx, " w=");
            buf_append_i32(msg, sizeof(msg), &idx, ms.wheel_delta);
        }
        desktop_append_log(msg);
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "savefs") || str_eq(p, "sync") || str_eq(p, "save")) {
        if (fs_persist_save_now()) {
            desktop_append_log("savefs: ramdisk image written");
        } else {
            desktop_append_log("savefs: failed (ata unavailable or write error)");
        }
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "betareport")) {
        if (write_beta_report_file()) {
            desktop_append_log("betareport: wrote beta_report.txt");
        } else {
            desktop_append_log("betareport: failed");
        }
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "loadfs")) {
        if (fs_persist_load_now()) {
            desktop_append_log("loadfs: ramdisk image restored");
        } else {
            desktop_append_log("loadfs: failed (missing image or read error)");
        }
        return CLI_ACTION_NONE;
    }

    if (starts_with(p, "ping ")) {
        p += 5;
        char ip_arg[32];
        if (!parse_arg(&p, ip_arg, sizeof(ip_arg))) {
            desktop_append_log("usage: ping <a.b.c.d>");
            return CLI_ACTION_NONE;
        }

        uint32_t ip_be = 0;
        if (!parse_ipv4(ip_arg, &ip_be)) {
            desktop_append_log("ping: invalid ipv4 address");
            return CLI_ACTION_NONE;
        }
        if (!net_stack_ready()) {
            desktop_append_log("ping: network stack unavailable");
            return CLI_ACTION_NONE;
        }
        if (net_stack_send_ping(ip_be)) {
            desktop_append_log("ping: echo request sent");
        } else {
            desktop_append_log("ping: send failed");
        }
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "clear") || str_eq(p, "cls")) {
        desktop_clear_log();
        return CLI_ACTION_NONE;
    }

    if (str_eq(p, "doom")) {
        desktop_append_log("launching id Software DOOM...");
        return CLI_ACTION_LAUNCH_DOOM;
    }

    desktop_append_log("unknown command");
    return CLI_ACTION_NONE;
}
