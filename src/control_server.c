// src/control_server.c
// Minimal telnet-like control server for SDL port.
// Enabled only when NOX_CONTROL_SERVER is truthy.
// Auth via NOX_CONTROL_SERVER_PASSWORD.
// No extra deps: uses SDL threads/mutex + BSD sockets.
#include "defs.h"
#ifdef USE_SDL

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <SDL2/SDL.h>


static int g_ctrllog(void) {
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("NOX_CONTROL_LOG");
        v = (e && *e && strcmp(e, "0") != 0) ? 1 : 0;
    }
    return v;
}

#ifndef NOX_CTRL_LOG
#define NOX_CTRL_LOG(fmt, ...)                                             \
    do {                                                                   \
        if (g_ctrllog())                                                   \
            fprintf(stderr, "[ctrl] " fmt "\n", ##__VA_ARGS__);            \
    } while (0)
#endif

static void handle_one_command(int fd, const char *cmd, int *authed, const char *pw);
static void send_str_maybe(int fd, const char *s);

// ------------------------------------------------------------
// Small env helpers (copied pattern from lobby.c, self-contained)
// ------------------------------------------------------------
static int nox_env_truthy(const char *s)
{
    if (!s) return 0;
    while (*s && (unsigned char)*s <= ' ') s++;
    if (!*s) return 0;

    if (s[0] == '1' && s[1] == 0) return 1;

    char buf[8];
    size_t n = 0;
    while (s[n] && n + 1 < sizeof(buf)) {
        buf[n] = (char)tolower((unsigned char)s[n]);
        n++;
    }
    buf[n] = 0;

    return (strcmp(buf, "true") == 0) ||
           (strcmp(buf, "yes")  == 0) ||
           (strcmp(buf, "on")   == 0);
}

static double nox_env_double(const char *name, double defv)
{
    const char *s = getenv(name);
    if (!s || !*s) return defv;
    char *end = NULL;
    double v = strtod(s, &end);
    if (end == s) return defv; // not a number
    return v;
}

static int clamp_int(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int g_sleep_scale_init = 0;
static double g_sleep_scale = 1.0;

static double nox_sleep_scale(void)
{
    if (!g_sleep_scale_init) {
        g_sleep_scale = nox_env_double("NOX_CONTROL_SERVER_SLEEP_SCALE", 1.0);
        if (g_sleep_scale < 0.1) g_sleep_scale = 0.1;
        if (g_sleep_scale > 50.0) g_sleep_scale = 50.0;
        g_sleep_scale_init = 1;
        NOX_CTRL_LOG("NOX_CONTROL_SERVER_SLEEP_SCALE=%f", g_sleep_scale);
    }
    return g_sleep_scale;
}

static int nox_env_int(const char *name, int defv)
{
    const char *s = getenv(name);
    if (!s || !*s) return defv;
    return atoi(s);
}

static const char *nox_env_str(const char *name, const char *defv)
{
    const char *s = getenv(name);
    return (s && *s) ? s : defv;
}

static void set_sock_timeouts(int fd, int ms)
{
    // Keep it simple; timeouts prevent stuck reads forever.
    struct timeval tv;
    tv.tv_sec  = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, (socklen_t)sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, (socklen_t)sizeof(tv));
}

// ------------------------------------------------------------
// Telnet-friendly line reader: ignores IAC negotiation.
// Terminates on '\n' or '\r' (accepts CRLF).
// Returns:
//   1 = line ok
//   0 = EOF
//  -1 = error/timeout
//   2 = "interrupt" (Ctrl+C / Telnet IP)
// ------------------------------------------------------------
static int recv_line_telnet(int fd, char *out, size_t cap)
{
    if (!out || cap == 0) return -1;

    size_t n = 0;

    for (;;) {
        unsigned char c = 0;
        ssize_t r = recv(fd, &c, 1, 0);
        if (r == 0) { out[n] = 0; return 0; }
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }

        // Raw Ctrl+C (ETX)
        if (c == 0x03) {
            out[0] = 0;
            return 2; // interrupt
        }

        // Telnet IAC handling
        if (c == 0xFF) {
            unsigned char b1 = 0;
            ssize_t r1 = recv(fd, &b1, 1, 0);
            if (r1 <= 0) continue;

            // Literal 0xFF (IAC IAC)
            if (b1 == 0xFF) {
                c = 0xFF; // treat as normal character below
            } else {
                // Telnet "Interrupt Process" (IAC IP) is 2 bytes: FF F4
                if (b1 == 0xF4) {
                    out[0] = 0;
                    return 2; // interrupt
                }

                // Most negotiation is 3 bytes: IAC <cmd> <opt>
                // For WILL/WONT/DO/DONT (FB/FC/FD/FE), consume option byte.
                if (b1 == 0xFB || b1 == 0xFC || b1 == 0xFD || b1 == 0xFE) {
                    unsigned char opt = 0;
                    (void)recv(fd, &opt, 1, 0);
                }

                // Ignore this telnet command sequence entirely
                continue;
            }
        }

        if (c == '\r' || c == '\n') {
            out[n] = 0;
            return 1;
        }

        if (c < 0x20 || c == 0x7F) {
            // Allow tab if you want:
            // if (c == '\t') { /* keep */ } else
            continue;
        }

        if (n + 1 < cap) {
            out[n++] = (char)c;
            out[n] = 0;
        } else {
            // truncate but keep reading until newline
        }
    }
}


// ------------------------------------------------------------
// Control action queue (network thread -> main thread)
// ------------------------------------------------------------
typedef enum {
    ACT_NONE = 0,
    ACT_MOVE,           // relative dx dy
    ACT_BTN,            // button(0/1/2) down(0/1)
    ACT_KEY,            // SDL scancode, down(0/1)
    ACT_TEXT,           // utf-8 string
    ACT_SLEEP_MS,       // delay
    ACT_HOME,           // slam to top-left
    ACT_TRHOME,         // slam to top-right (best-effort)
    ACT_QUIT_GAME,
    ACT_LOG
//    ACT_MACRO_START_SERVER
} ActionType;



typedef struct {
    ActionType type;
    int a, b;
    int down;
    char text[256];
} ControlAction;

enum { QCAP = 1024 };
static ControlAction g_q[QCAP];
static int g_qr = 0, g_qw = 0;
static SDL_mutex *g_qmu = NULL;

static int q_push(const ControlAction *in)
{
    if (!g_qmu) {
        NOX_CTRL_LOG("q_push: no mutex (not initialized?)");
        return 0;
    }

    SDL_LockMutex(g_qmu);
    int next = (g_qw + 1) % QCAP;
    if (next == g_qr) {
        SDL_UnlockMutex(g_qmu);
        NOX_CTRL_LOG("q_push: queue FULL (dropping action type=%d)", in ? (int)in->type : -1);
        return 0;
    }
    g_q[g_qw] = *in;
    g_qw = next;
    SDL_UnlockMutex(g_qmu);
    return 1;
}

static int q_pop(ControlAction *out)
{
    if (!g_qmu) return 0;
    SDL_LockMutex(g_qmu);
    if (g_qr == g_qw) {
        SDL_UnlockMutex(g_qmu);
        return 0;
    }
    *out = g_q[g_qr];
    g_qr = (g_qr + 1) % QCAP;
    SDL_UnlockMutex(g_qmu);
    return 1;
}

// ------------------------------------------------------------
// We inject into your existing input.c queues.
// Declare tiny injection functions we will add to input.c.
// ------------------------------------------------------------
extern void nox_ctrl_inject_mouse_move(int dx, int dy, int wheel);
extern void nox_ctrl_inject_mouse_button(int button /*0/1/2*/, int down);
extern void nox_ctrl_inject_key_scancode(int sdl_scancode, int down);
extern void nox_ctrl_inject_text_utf8(const char *utf8);

// Capture hook (we will add in input.c too):
extern void nox_ctrl_capture_event(const SDL_Event *ev);
extern int  nox_ctrl_capture_enabled(void);

// ------------------------------------------------------------
// Command parsing helpers
// ------------------------------------------------------------
static const char *skip_ws(const char *p)
{
    while (p && *p && (unsigned char)*p <= ' ') p++;
    return p;
}

static int streq_ci(const char *a, const char *b)
{
    if (!a || !b) return 0;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

static int parse_int(const char *p, int *out, const char **endp)
{
    p = skip_ws(p);
    int sign = 1;
    if (*p == '-') { sign = -1; p++; }
    if (!isdigit((unsigned char)*p)) return 0;
    long v = 0;
    while (isdigit((unsigned char)*p)) {
        v = v * 10 + (*p - '0');
        p++;
        if (v > 1000000) break;
    }
    if (out) *out = (int)(v * sign);
    if (endp) *endp = p;
    return 1;
}

static int parse_quoted(const char *p, char *out, size_t cap, const char **endp)
{
    p = skip_ws(p);
    if (*p != '"') return 0;
    p++;
    size_t n = 0;
    while (*p && *p != '"') {
        char c = *p++;
        if (c == '\\' && *p) {
            char e = *p++;
            if (e == '"' || e == '\\') c = e;
            else c = e;
        }
        if (n + 1 < cap) out[n++] = c;
    }
    if (*p != '"') return 0;
    p++;
    if (cap) out[n < cap ? n : (cap - 1)] = 0;
    if (endp) *endp = p;
    return 1;
}

// Split commands by ';' not inside quotes.
// Modifies buf in place: replaces separators with '\0' and outputs pointers.
static int split_commands(char *buf, char **cmds, int max_cmds)
{
    int n = 0;
    int in_q = 0;
    char *start = buf;

    for (char *p = buf; *p; ++p) {
        if (*p == '"' && (p == buf || p[-1] != '\\')) in_q = !in_q;
        if (!in_q && *p == ';') {
            *p = 0;
            if (n < max_cmds) cmds[n++] = start;
            start = p + 1;
        }
    }
    if (n < max_cmds) cmds[n++] = start;
    return n;
}

// Strip comments starting with '#' (only when not inside quotes).
// Modifies s in place.
static void strip_hash_comment(char *s)
{
    if (!s) return;
    int in_q = 0;
    for (char *p = s; *p; ++p) {
        if (*p == '"' && (p == s || p[-1] != '\\')) in_q = !in_q;
        if (!in_q && *p == '#') {
            *p = 0;
            return;
        }
    }
}


static void nox_control_server_bootstrap_from_env(void)
{
    const char *s0 = getenv("NOX_CONTROL_SERVER_BOOT");
    if (!s0 || !*s0) return;

    // Copy because we'll edit it in-place for splitting.
    char buf[2048];
    strncpy(buf, s0, sizeof(buf)-1);
    buf[sizeof(buf)-1] = 0;

    // Allow newlines as separators too.
    for (char *p = buf; *p; ++p) {
        if (*p == '\n' || *p == '\r') *p = ';';
    }

    // Split into commands just like socket input.
    char *cmds[64];
    int ncmd = split_commands(buf, cmds, (int)(sizeof(cmds)/sizeof(cmds[0])));

    // Env is trusted-local, so treat as authed.
    int authed = 1;
    const char *pw = getenv("NOX_CONTROL_SERVER_PASSWORD"); // unused when authed=1

    NOX_CTRL_LOG("BOOT: executing NOX_CONTROL_SERVER_BOOT (%d cmds)", ncmd);

    for (int i = 0; i < ncmd; ++i) {
        char *c = cmds[i];
        if (!c) continue;

        // trim
        while (*c && (unsigned char)*c <= ' ') c++;
        char *ce = c + strlen(c);
        while (ce > c && (unsigned char)ce[-1] <= ' ') *--ce = 0;
        if (!*c) continue;

        strip_hash_comment(c);

        // fd = -1 means "don't write responses"
        handle_one_command(-1, c, &authed, pw);
    }
}


// ------------------------------------------------------------
// Absolute-click tracking (Plan A)
// ------------------------------------------------------------
static int g_ctrl_x = 0;
static int g_ctrl_y = 0;

static int g_tr_x = 0;
static int g_tr_y = 0;

static int g_trhome_left = 0;

static void enqueue_trhome(void)
{
    ControlAction a;
    memset(&a, 0, sizeof(a));
    a.type = ACT_TRHOME;
    q_push(&a);

    g_tr_x = 0;
    g_tr_y = 0;
}

static void enqueue_quit_game(void)
{
    ControlAction a;
    memset(&a, 0, sizeof(a));
    a.type = ACT_QUIT_GAME;
    q_push(&a);
}

static void enqueue_log(const char *msg)
{
    ControlAction a;
    memset(&a, 0, sizeof(a));
    a.type = ACT_LOG;
    strncpy(a.text, msg ? msg : "", sizeof(a.text) - 1);
    a.text[sizeof(a.text) - 1] = 0;
    q_push(&a);
}

// Move by absolute TR coordinate (x,y) using tracker deltas.
static void enqueue_trclick_xy(int x, int y, int right)
{
    // Convert TR target delta into SDL relative delta:
    // TR +x is left => SDL dx is negative.
    // TR +y is down => SDL dy is positive.
    int sdl_dx = -(x - g_tr_x);
    int sdl_dy =  (y - g_tr_y);

    ControlAction a;
    memset(&a, 0, sizeof(a));

    a.type = ACT_MOVE;
    a.a = sdl_dx;
    a.b = sdl_dy;
    q_push(&a);

    a.type = ACT_BTN;
    a.a = right ? 1 : 0; // 0=left,1=right
    a.down = 1;
    q_push(&a);

    // Give the engine at least one pump/tick to see "down" before "up".
    a.type = ACT_SLEEP_MS;
    a.a = 16; // ~1 frame at 60Hz
    q_push(&a);

    a.type = ACT_BTN;
    a.a = right ? 1 : 0;
    a.down = 0;
    q_push(&a);

    g_tr_x = x;
    g_tr_y = y;
}

// TR relative move (dx,dy) where dx>0 means left
static void enqueue_trmove_rel(int dx, int dy)
{
    ControlAction a;
    memset(&a, 0, sizeof(a));

    a.type = ACT_MOVE;
    a.a = -dx; // TR +dx is left => SDL dx negative
    a.b =  dy; // TR +dy is down => SDL dy positive
    q_push(&a);

    g_tr_x += dx;
    g_tr_y += dy;
}

// Click by absolute coordinate using internal tracker:
// move (x - g_ctrl_x, y - g_ctrl_y), then ldown/lup, set tracker.
static void enqueue_click_abs(int x, int y, int right)
{
    ControlAction a;
    memset(&a, 0, sizeof(a));

    // Always re-anchor at top-left so click coords are absolute from (0,0).
    a.type = ACT_HOME;
    q_push(&a);

    // After home, our "tracked" position is top-left.
    g_ctrl_x = 0;
    g_ctrl_y = 0;

    // Move from (0,0) -> (x,y) using relative motion.
    a.type = ACT_MOVE;
    a.a = x;
    a.b = y;
    q_push(&a);

    // Button down
    a.type = ACT_BTN;
    a.a = right ? 1 : 0; // 0=left,1=right,2=middle
    a.down = 1;
    q_push(&a);

    // Give the engine at least one pump/tick to see "down" before "up".
    a.type = ACT_SLEEP_MS;
    a.a = 16; // ~1 frame at 60Hz
    q_push(&a);

    // Button up
    a.type = ACT_BTN;
    a.a = right ? 1 : 0;
    a.down = 0;
    q_push(&a);

    // Update tracker to the absolute coordinate we believe we're at now.
    g_ctrl_x = x;
    g_ctrl_y = y;
}

static void enqueue_home(void)
{
    ControlAction a;
    memset(&a, 0, sizeof(a));
    a.type = ACT_HOME;
    q_push(&a);
    g_ctrl_x = 0;
    g_ctrl_y = 0;
}

// ------------------------------------------------------------
// Hard-coded macro: start_server (stub sequence for now)
// You will fill real coordinates once you know UI.
// ------------------------------------------------------------
static void enqueue_macro_start_server(void)
{
//    // Example skeleton:
//    // home; click 100 420; sleep 150; click 320 240; ...
//    enqueue_home();
//
//    ControlAction a;
//    memset(&a, 0, sizeof(a));
//
//    // TODO: replace these with real UI positions/actions.
//    // For now it's just a placeholder so the plumbing is in place.
//    // e.g. "Internet Game" button etc.
//    enqueue_click_abs(100, 420, 0);
//    a.type = ACT_SLEEP_MS; a.a = 150; q_push(&a);
//    enqueue_click_abs(320, 240, 0);
}

// ------------------------------------------------------------
// Built-in macro support
// ------------------------------------------------------------
typedef struct {
    const char *name;
    const char *script; // semicolon-separated commands, may include # comments
} NoxCtrlMacro;

static const NoxCtrlMacro g_macros[] = {
    {
        "startMultiplayerNetworkHost",
        "sleep 1000; c 1 1; sleep 1000; c 1 1; sleep 1000; c 1 1; sleep 1000; "
        "c 250 165; sleep 2000; c 300 270; sleep 3000; click 60 60; sleep 2000; "
        "# startMultiplayerNetworkHost\n"
    },
    {
        "newMultiNewCharacterWarrior",
        "c 200 380; sleep 2000; c 200 200; sleep 1000; c 350 400; sleep 2000; "
        "c 160 270; bs8; type \"server\"; c 240 380; sleep 2000 "
        "# newMultiNewCharacterWarrior\n"
    },
    {
        "chatScreenPopUpClickOk",
        "c 530 460; sleep 1000; # chatScreenPopUpClickOk\n"
    },
    {
        "chatScreenServerName",
        "trh; trc 60 40; sleep 1000; bs16; # chatScreenServerName\n"
    },
    {
        "chatScreenServerTab",
        "trh; trc 80 0; # chatScreenServerTab\n"
    },
    {
            "server",
            "macro startMultiplayerNetworkHost;"
            "macro newMultiNewCharacterWarrior;"
            "macro chatScreenPopUpClickOk;"
            "macro chatScreenServerName; "
            "type \"${NOX_SERVER_NAME:NoxDecompServ}\"; sleep 1000;"
            "key esc; sleep 1000;"
            "macro defaultServerGame;\n"
    },
    {
        "defaultServerGame",
        "key F1; sleep 1500;"
        "t \"racoiaws\"; sleep 1000;"
        "t \"set sysop ${NOX_SERVER_SYSOP:secret}\"; sleep 1500;"
        "t \"set lessons ${NOX_SERVER_LESSONS:15}\"; sleep 1000;"
        "t \"set time ${NOX_SERVER_TIME:0}\"; sleep 2000;"
        "t \"load ${NOX_SERVER_DEFAULT_MAP:capflag}\"; sleep 5000;"
        "key F1; sleep 1000;\n"
    },
    {
        "gameControl",
        "key esc; c 550 120; # main menu and game control button\n"
    },
    {
        "gameControlFormNewGame",
        "trh; trc 180 350\n"
    },
    {
        "gameControlGoButton",
        "trh; trc 185 375\n"
    },
};

static const NoxCtrlMacro *find_macro(const char *name)
{
    if (!name || !*name) return NULL;
    for (size_t i = 0; i < sizeof(g_macros)/sizeof(g_macros[0]); i++) {
        if (streq_ci(g_macros[i].name, name))
            return &g_macros[i];
    }
    return NULL;
}

static void list_macros(int fd)
{
    send_str_maybe(fd, "Macros:\r\n");
    for (size_t i = 0; i < sizeof(g_macros)/sizeof(g_macros[0]); i++) {
        send_str_maybe(fd, "  ");
        send_str_maybe(fd, g_macros[i].name);
        send_str_maybe(fd, "\r\n");
    }
}

// Expand ${VARNAME} using getenv(). Unknown/unset -> "" (or keep default if you want).
static const char *env_or_empty(const char *name)
{
    const char *v = getenv(name);
    return (v && *v) ? v : "";
}

//static void expand_env_vars(const char *in, char *out, size_t out_cap)
//{
//    if (!out || out_cap == 0) return;
//    out[0] = 0;
//    if (!in) return;
//
//    size_t oi = 0;
//    for (size_t i = 0; in[i] && oi + 1 < out_cap; ) {
//        // Look for ${...}
//        if (in[i] == '$' && in[i+1] == '{') {
//            size_t j = i + 2;
//            char var[128];
//            size_t vn = 0;
//
//            while (in[j] && in[j] != '}' && vn + 1 < sizeof(var)) {
//                var[vn++] = in[j++];
//            }
//            var[vn] = 0;
//
//            if (in[j] == '}') {
//                const char *val = env_or_empty(var);
//                for (size_t k = 0; val[k] && oi + 1 < out_cap; k++) {
//                    out[oi++] = val[k];
//                }
//                out[oi] = 0;
//                i = j + 1;
//                continue;
//            }
//            // malformed ${... (no closing brace) -> treat as normal char '$'
//        }
//
//        out[oi++] = in[i++];
//        out[oi] = 0;
//    }
//}

// Expand ${VAR} or ${VAR:default} using getenv(VAR).
// - If VAR is unset/empty/whitespace => use default if present, else "".
// - Writes into out (cap bytes). Always NUL-terminates.
// Returns number of bytes written (excluding NUL).
static size_t expand_env_vars(const char *in, char *out, size_t cap)
{
    if (!out || cap == 0) return 0;
    out[0] = 0;
    if (!in) return 0;

    size_t w = 0;

    for (const char *p = in; *p; ) {
        // Copy normal chars fast-path
        if (!(p[0] == '$' && p[1] == '{')) {
            if (w + 1 < cap) out[w++] = *p;
            p++;
            continue;
        }

        // We saw "${"
        const char *start = p;   // for fallback copy
        p += 2;

        // Parse VAR name [A-Z0-9_]+
        char var[128];
        size_t vn = 0;
        while (*p && *p != '}' && *p != ':' && vn + 1 < sizeof(var)) {
            char c = *p;
            if (!(isalnum((unsigned char)c) || c == '_')) break;
            var[vn++] = c;
            p++;
        }
        var[vn] = 0;

        // If no valid var name, treat literally
        if (vn == 0) {
            // copy "${" literally and continue from after it
            if (w + 2 < cap) { out[w++] = '$'; out[w++] = '{'; }
            continue;
        }

        // Optional default after ':'
        const char *def = NULL;
        size_t deflen = 0;
        if (*p == ':') {
            p++; // skip ':'
            def = p;
            while (*p && *p != '}') p++;
            deflen = (size_t)(p - def);
        } else {
            // Skip any junk until '}' or end (keeps behavior forgiving)
            while (*p && *p != '}') p++;
        }

        if (*p != '}') {
            // No closing brace: fallback to literal from start
            p = start;
            if (w + 1 < cap) out[w++] = *p++;
            continue;
        }
        p++; // consume '}'

        // Lookup env
        const char *val = getenv(var);
        if (val) {
            // trim leading whitespace
            while (*val && (unsigned char)*val <= ' ') val++;
        }
        if (!val || !*val) {
            val = NULL;
        }

        if (!val) {
            // use default if present, else empty
            if (def && deflen) {
                for (size_t i = 0; i < deflen; i++) {
                    if (w + 1 < cap) out[w++] = def[i];
                }
            }
        } else {
            // copy env value
            for (const char *q = val; *q; q++) {
                if (w + 1 < cap) out[w++] = *q;
            }
        }
    }

    if (cap) out[(w < cap) ? w : (cap - 1)] = 0;
    return w;
}


static void run_script_as_commands(int fd, const char *script, int *authed, const char *pw)
{
    if (!script || !*script) return;

    // 1) Expand ${VARS} into an intermediate buffer
    char expanded[8192];
    expand_env_vars(script, expanded, sizeof(expanded));

    // 2) Now operate on expanded script
    char buf[8192];
    strncpy(buf, expanded, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;

    // normalize newlines into separators
    for (char *p = buf; *p; ++p) {
        if (*p == '\n' || *p == '\r') *p = ';';
    }

    // IMPORTANT: strip trailing comments (works per-line, but after newline->';' it’s fine)
    // We'll strip per command chunk too, below.

    char *cmds[128];
    int ncmd = split_commands(buf, cmds, (int)(sizeof(cmds)/sizeof(cmds[0])));

    for (int i = 0; i < ncmd; i++) {
        char *c = cmds[i];
        if (!c) continue;

        // trim
        while (*c && (unsigned char)*c <= ' ') c++;
        char *ce = c + strlen(c);
        while (ce > c && (unsigned char)ce[-1] <= ' ') *--ce = 0;
        if (!*c) continue;

        // strip #comment outside quotes
        strip_hash_comment(c);

        // re-trim after stripping
        ce = c + strlen(c);
        while (ce > c && (unsigned char)ce[-1] <= ' ') *--ce = 0;
        if (!*c) continue;

        // Run through the normal handler (same parsing/queue behavior)
        handle_one_command(fd, c, authed, pw);
    }
}


// ------------------------------------------------------------
// Server state
// ------------------------------------------------------------
static SDL_Thread *g_thr = NULL;
static int g_server_fd = -1;
static int g_client_fd = -1;
static volatile int g_running = 0;

static int send_str(int fd, const char *s)
{
    if (fd < 0 || !s) return -1;

    size_t len = strlen(s);
    while (len) {
        int flags = 0;
#ifdef MSG_NOSIGNAL
        flags |= MSG_NOSIGNAL; // avoid SIGPIPE on Linux
#endif
        ssize_t n = send(fd, s, len, flags);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1; // EPIPE/ECONNRESET/etc
        }
        s += (size_t)n;
        len -= (size_t)n;
    }
    return 0;
}

static void send_str_maybe(int fd, const char *s)
{
    if (fd >= 0 && s) (void)send_str(fd, s);
}


static void close_fd(int *pfd)
{
    if (pfd && *pfd >= 0) {
        close(*pfd);
        *pfd = -1;
    }
}

static int make_listener(const char *bind_ip, int port)
{
    int fd = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, (socklen_t)sizeof(one));

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, bind_ip, &sa.sin_addr) != 1) {
        close(fd);
        return -1;
    }

    if (bind(fd, (struct sockaddr *)&sa, (socklen_t)sizeof(sa)) < 0) {
        close(fd);
        return -1;
    }
    if (listen(fd, 4) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void help_banner(int fd)
{
    send_str_maybe(fd,
        "NOX control server\r\n"
        "Commands:\r\n"
        "  auth <password>\r\n"
        "  home | h\r\n"
        "  rmove <dx> <dy>      (relative)\r\n"
        "  move <x> <y> | m     (home-first absolute move)\r\n"
        "  ldown|lup|rdown|rup|mdown|mup\r\n"
        "  click <x> <y> | c    (homes first)\r\n"
        "  rclick <x> <y>\r\n"
        "  trhome | trh (homes to top right corner)\r\n"
        "  trmove <dx> <dy> | trm <dx> <dy>   (dx>0 = left, dy>0 = down)\r\n"
        "  trclick <x> <y> | trc <x> <y>      (x left, y down from top-right)\r\n"
        "  f1\r\n"
        "  keydown <name> | keyup <name> | key <name>\r\n"
        "  type \"text\"   or just \"text\"\r\n"
        "  sleep <ms>\r\n"
        "  killgame            (terminate the whole game process)\r\n"
        "  macros            (list built-in macros)\r\n"
        "  macro <name>      (run a built-in macro)\r\n"
        "  quit\r\n"
        "Multiple commands per line: separate with ';' (quotes supported).\r\n"
    );
}

static int key_name_to_scancode(const char *name)
{
    if (!name || !*name) return SDL_SCANCODE_UNKNOWN;

    // Common ones
    if (streq_ci(name, "F1")) return SDL_SCANCODE_F1;
    if (streq_ci(name, "ESC") || streq_ci(name, "ESCAPE")) return SDL_SCANCODE_ESCAPE;
    if (streq_ci(name, "ENTER") || streq_ci(name, "RETURN")) return SDL_SCANCODE_RETURN;
    if (streq_ci(name, "SPACE")) return SDL_SCANCODE_SPACE;
    if (streq_ci(name, "TAB")) return SDL_SCANCODE_TAB;
    if (streq_ci(name, "BACKSPACE")) return SDL_SCANCODE_BACKSPACE;

    // Letters A-Z
    if (strlen(name) == 1 && isalpha((unsigned char)name[0])) {
        char c = (char)toupper((unsigned char)name[0]);
        return SDL_SCANCODE_A + (c - 'A');
    }

    // Digits 0-9
    if (strlen(name) == 1 && isdigit((unsigned char)name[0])) {
        char d = name[0];
        if (d == '0') return SDL_SCANCODE_0;
        return SDL_SCANCODE_1 + (d - '1');
    }

    return SDL_SCANCODE_UNKNOWN;
}

// ------------------------------------------------------------
// Key helper: enqueue a press (down+up) for a scancode
// ------------------------------------------------------------
static void enqueue_key_press(int scancode)
{
    ControlAction a;
    memset(&a, 0, sizeof(a));
    a.type = ACT_KEY;
    a.a = scancode;

    // key down
    a.down = 1;
    q_push(&a);

    // give the game at least one pump/frame to observe it
    memset(&a, 0, sizeof(a));
    a.type = ACT_SLEEP_MS;
    a.a = 16; // ~1 frame @ 60Hz
    q_push(&a);

    // key up
    memset(&a, 0, sizeof(a));
    a.type = ACT_KEY;
    a.a = scancode;
    a.down = 0;
    q_push(&a);
}

static void enqueue_key_tap_safe(int scancode)
{
    ControlAction a;

    // 1) Ensure it's not stuck down
    memset(&a, 0, sizeof(a));
    a.type = ACT_KEY;
    a.a = scancode;
    a.down = 0;
    q_push(&a);

    // small gap so the engine consumes the key-up first
    memset(&a, 0, sizeof(a));
    a.type = ACT_SLEEP_MS;
    a.a = 16;
    q_push(&a);

    // 2) Normal tap (your existing down/sleep/up)
    enqueue_key_press(scancode);

    // 3) Optional post-gap so rapid repeats are distinct
    memset(&a, 0, sizeof(a));
    a.type = ACT_SLEEP_MS;
    a.a = 16;
    q_push(&a);
}


static void enqueue_backspaces(int count)
{
    if (count < 0) count = 0;
    if (count > 512) count = 512;
    for (int i = 0; i < count; i++)
        enqueue_key_press(SDL_SCANCODE_BACKSPACE);
}

// Minimal "type via keys" - supports: a-z A-Z 0-9 space.
// For A-Z we hold shift around the key press.
static void enqueue_type_via_keys(const char *text)
{
    if (!text) return;

    for (const unsigned char *p = (const unsigned char*)text; *p; ++p) {
        unsigned char c = *p;

        if (c == ' ') {
            enqueue_key_press(SDL_SCANCODE_SPACE);
            continue;
        }

        if (c >= '0' && c <= '9') {
            int sc;
            if (c == '0') sc = SDL_SCANCODE_0;
            else          sc = SDL_SCANCODE_1 + (c - '1'); // 1..9 contiguous
            enqueue_key_press(sc);
            continue;
        }

        if (c >= 'a' && c <= 'z') {
            enqueue_key_press(SDL_SCANCODE_A + (c - 'a'));
            continue;
        }

        if (c >= 'A' && c <= 'Z') {
            // SHIFT + letter
            ControlAction a;
            memset(&a, 0, sizeof(a));
            a.type = ACT_KEY;

            a.a = SDL_SCANCODE_LSHIFT;
            a.down = 1; q_push(&a);

            enqueue_key_press(SDL_SCANCODE_A + (c - 'A'));

            a.a = SDL_SCANCODE_LSHIFT;
            a.down = 0; q_push(&a);
            continue;
        }

        // Optional: handle \n as Enter
        if (c == '\n' || c == '\r') {
            enqueue_key_press(SDL_SCANCODE_RETURN);
            continue;
        }

        // Unsupported punctuation for now.
        // You can expand this later (minus, period, slash, etc) if needed.
        NOX_CTRL_LOG("type: unsupported char 0x%02x '%c' (skipping)", c, (c >= 32 && c < 127) ? c : '?');
    }
}


static void handle_one_command(int fd, const char *cmd, int *authed, const char *pw)
{
    char tmp[512];
    strncpy(tmp, cmd ? cmd : "", sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = 0;

    char *p = tmp;
    p = (char*)skip_ws(p);
    if (!*p) return;

    // quoted shorthand -> type "..."
    if (*p == '"') {
        char text[256];
        const char *endp = NULL;
        if (parse_quoted(p, text, sizeof(text), &endp)) {
            enqueue_type_via_keys(text);
            send_str_maybe(fd, "OK\r\n");
        } else {
            send_str_maybe(fd, "ERR bad string\r\n");
        }
        return;
    }

    // token = first word
    char *tok = p;
    while (*p && (unsigned char)*p > ' ') p++;
    if (*p) *p++ = 0;
    p = (char*)skip_ws(p);

    // ------------------------------------------------------------
    // Aliases + renames
    //   h -> home
    //   c -> click
    //   m -> move   (home-first absolute move)
    //   move(relative) renamed to rmove
    // ------------------------------------------------------------
    if (streq_ci(tok, "h")) tok = "home";
    if (streq_ci(tok, "c")) tok = "click";
    if (streq_ci(tok, "m")) tok = "move";

    if (streq_ci(tok, "help")) { help_banner(fd); return; }
    if (streq_ci(tok, "ping")) { send_str_maybe(fd, "pong\r\n"); return; }
    if (streq_ci(tok, "quit") || streq_ci(tok, "exit")) {
        send_str_maybe(fd, "bye\r\n");
        if (fd >= 0) shutdown(fd, SHUT_RDWR);
        close_fd(&g_client_fd); // also sets to -1
        return;
    }

    if (streq_ci(tok, "auth")) {
        if (!pw || !*pw) { send_str_maybe(fd, "ERR no password set\r\n"); return; }
        if (p && strcmp(p, pw) == 0) { *authed = 1; send_str_maybe(fd, "OK authed\r\n"); }
        else { send_str_maybe(fd, "ERR bad password\r\n"); }
        return;
    }

    if (!*authed) { send_str_maybe(fd, "ERR not authed\r\n"); return; }

    if (streq_ci(tok, "killgame") || streq_ci(tok, "gamekill")) {
        enqueue_quit_game();
        send_str_maybe(fd, "OK quitting game\r\n");
        return;
    }

    // Commands
    if (streq_ci(tok, "home")) {
        enqueue_home();
        send_str_maybe(fd, "OK\r\n");
        return;
    }

    // --- Top-right anchored commands (TR) ---
    if (streq_ci(tok, "trhome") || streq_ci(tok, "trh")) {
        enqueue_trhome();
        send_str(fd, "OK\r\n");
        return;
    }

    // TR relative move: dx>0 means left, dy>0 means down
    if (streq_ci(tok, "trmove") || streq_ci(tok, "trm")) {
        int dx=0, dy=0;
        const char *ep = NULL;
        if (!parse_int(p, &dx, &ep)) { send_str(fd, "ERR trmove dx dy\r\n"); return; }
        if (!parse_int(ep, &dy, &ep)) { send_str(fd, "ERR trmove dx dy\r\n"); return; }
        enqueue_trmove_rel(dx, dy);
        send_str(fd, "OK\r\n");
        return;
    }

    // TR click at coordinate (x,y): x pixels left from top-right, y pixels down
    if (streq_ci(tok, "trclick") || streq_ci(tok, "trc")) {
        int x=0, y=0;
        const char *ep = NULL;
        if (!parse_int(p, &x, &ep)) { send_str(fd, "ERR trclick x y\r\n"); return; }
        if (!parse_int(ep, &y, &ep)) { send_str(fd, "ERR trclick x y\r\n"); return; }
        enqueue_trclick_xy(x, y, 0);
        send_str(fd, "OK\r\n");
        return;
    }


    // move <x> <y> : home-first absolute move from (0,0) -> (x,y)
    if (streq_ci(tok, "move")) {
        int x=0, y=0;
        const char *ep = NULL;
        if (!parse_int(p, &x, &ep)) { send_str_maybe(fd, "ERR move x y\r\n"); return; }
        if (!parse_int(ep, &y, &ep)) { send_str_maybe(fd, "ERR move x y\r\n"); return; }

        // enqueue home
        ControlAction a; memset(&a,0,sizeof(a));
        a.type = ACT_HOME;
        q_push(&a);

        // tracker reset
        g_ctrl_x = 0;
        g_ctrl_y = 0;

        // move to absolute (x,y) using relative motion from top-left
        a.type = ACT_MOVE;
        a.a = x;
        a.b = y;
        q_push(&a);

        g_ctrl_x = x;
        g_ctrl_y = y;

        send_str_maybe(fd, "OK\r\n");
        return;
    }

    // rmove <dx> <dy> : relative move
    if (streq_ci(tok, "rmove")) {
        int dx=0, dy=0;
        const char *ep = NULL;
        if (!parse_int(p, &dx, &ep)) { send_str_maybe(fd, "ERR rmove dx dy\r\n"); return; }
        if (!parse_int(ep, &dy, &ep)) { send_str_maybe(fd, "ERR rmove dx dy\r\n"); return; }

        ControlAction a; memset(&a,0,sizeof(a));
        a.type = ACT_MOVE; a.a = dx; a.b = dy;
        q_push(&a);

        g_ctrl_x += dx;
        g_ctrl_y += dy;

        send_str_maybe(fd, "OK\r\n");
        return;
    }

    if (streq_ci(tok, "sleep")) {
        int ms=0;
        const char *ep=NULL;
        if (!parse_int(p, &ms, &ep)) { send_str_maybe(fd, "ERR sleep ms\r\n"); return; }

        if (ms < 0) ms = 0;

        double scale = nox_sleep_scale();
        int scaled = (int)(ms * scale + 0.5);   // round
        scaled = clamp_int(scaled, 0, 60000);   // keep your existing cap

        ControlAction a; memset(&a,0,sizeof(a));
        a.type = ACT_SLEEP_MS; a.a = scaled;
        q_push(&a);

        send_str_maybe(fd, "OK\r\n");
        return;
    }

    if (streq_ci(tok, "ldown")||streq_ci(tok,"lup")||
        streq_ci(tok, "rdown")||streq_ci(tok,"rup")||
        streq_ci(tok, "mdown")||streq_ci(tok,"mup")) {
        int btn = 0;
        if (tok[0]=='r' || tok[0]=='R') btn = 1;
        else if (tok[0]=='m' || tok[0]=='M') btn = 2;
        int down = (tolower((unsigned char)tok[1])=='d'); // down vs up
        ControlAction a; memset(&a,0,sizeof(a));
        a.type = ACT_BTN; a.a = btn; a.down = down;
        q_push(&a);
        send_str_maybe(fd, "OK\r\n");
        return;
    }

    if (streq_ci(tok, "click") || streq_ci(tok, "rclick")) {
        int x=0,y=0;
        const char *ep=NULL;
        if (!parse_int(p, &x, &ep)) { send_str_maybe(fd, "ERR click x y\r\n"); return; }
        if (!parse_int(ep, &y, &ep)) { send_str_maybe(fd, "ERR click x y\r\n"); return; }
        enqueue_click_abs(x, y, streq_ci(tok, "rclick"));
        send_str_maybe(fd, "OK\r\n");
        return;
    }

    if (streq_ci(tok, "f1")) {
        enqueue_key_tap_safe(SDL_SCANCODE_F1);
        send_str_maybe(fd, "OK\r\n");
        return;
    }


    if (streq_ci(tok, "keydown") || streq_ci(tok, "keyup") || streq_ci(tok, "key")) {
        if (!p || !*p) { send_str_maybe(fd, "ERR key <name>\r\n"); return; }
        int sc = key_name_to_scancode(p);
        if (sc == SDL_SCANCODE_UNKNOWN) { send_str_maybe(fd, "ERR unknown key\r\n"); return; }

        if (streq_ci(tok, "key")) {
            enqueue_key_tap_safe(sc);
        } else {
            ControlAction a; memset(&a,0,sizeof(a));
            a.type = ACT_KEY; a.a = sc;
            a.down = streq_ci(tok, "keydown") ? 1 : 0;
            q_push(&a);

            // If you want keydown/keyup to also be reliable across frames:
            // (optional) add a tiny sleep after each
            // ControlAction s; memset(&s,0,sizeof(s)); s.type=ACT_SLEEP_MS; s.a=1; q_push(&s);
        }

        send_str_maybe(fd, "OK\r\n");
        return;
    }

    if (streq_ci(tok, "type")) {
        char text[256];
        const char *endp = NULL;
        if (!parse_quoted(p, text, sizeof(text), &endp)) { send_str_maybe(fd, "ERR type \"text\"\r\n"); return; }
        enqueue_type_via_keys(text);
        send_str_maybe(fd, "OK\r\n");
        return;
    }

        // t "text"  (type + press Enter)
        if (streq_ci(tok, "t")) {
            char text[256];
            const char *endp = NULL;
            if (!parse_quoted(p, text, sizeof(text), &endp)) { send_str_maybe(fd, "ERR t \"text\"\r\n"); return; }
            enqueue_type_via_keys(text);
            enqueue_key_press(SDL_SCANCODE_RETURN);
            send_str_maybe(fd, "OK\r\n");
            return;
        }

    if (streq_ci(tok, "bs8")) {
        enqueue_backspaces(8);
        send_str_maybe(fd, "OK\r\n");
        return;
    }

    if (streq_ci(tok, "bs16")) {
        enqueue_backspaces(16);
        send_str_maybe(fd, "OK\r\n");
        return;
    }

//    if (streq_ci(tok, "start_server")) {
//        ControlAction a; memset(&a,0,sizeof(a));
//        a.type = ACT_MACRO_START_SERVER;
//        q_push(&a);
//        send_str_maybe(fd, "OK\r\n");
//        return;
//    }
    if (streq_ci(tok, "macros")) {
        list_macros(fd);
        return;
    }

    if (streq_ci(tok, "macro")) {
        if (!p || !*p) { send_str_maybe(fd, "ERR macro <name>\r\n"); return; }

        const NoxCtrlMacro *m = find_macro(p);
        if (!m) { send_str_maybe(fd, "ERR unknown macro\r\n"); return; }

        send_str_maybe(fd, "Running macro: ");
        send_str_maybe(fd, m->name);
        send_str_maybe(fd, "\r\n");

        char buf[300];
        snprintf(buf, sizeof(buf), "macro begin: %s", m->name);
        enqueue_log(buf);

        run_script_as_commands(fd, m->script, authed, pw);
        send_str_maybe(fd, "OK\r\n");

        snprintf(buf, sizeof(buf), "macro end: %s", m->name);
        enqueue_log(buf);
        return;
    }

    // Optional: allow calling macro just by typing its name (nice for telnet)
    {
        const NoxCtrlMacro *m = find_macro(tok);
        if (m) {
            run_script_as_commands(fd, m->script, authed, pw);
            send_str_maybe(fd, "OK\r\n");
            return;
        }
    }


    send_str_maybe(fd, "ERR unknown command\r\n");
}

static int server_thread(void *unused)
{
    (void)unused;

    const char *pw = getenv("NOX_CONTROL_SERVER_PASSWORD");
    const char *bind_ip = nox_env_str("NOX_CONTROL_SERVER_BIND", "127.0.0.1");
    int port = nox_env_int("NOX_CONTROL_SERVER_PORT", 2323);

    NOX_CTRL_LOG("thread: start (bind=%s port=%d pw=%s)",
                 bind_ip, port, (pw && *pw) ? "(set)" : "(missing/empty)");

    if (!pw || !*pw) {
        NOX_CTRL_LOG("thread: NOX_CONTROL_SERVER_PASSWORD not set; exiting");
        g_running = 0;
        return 0;
    }

    g_server_fd = make_listener(bind_ip, port);
    if (g_server_fd < 0) {
        NOX_CTRL_LOG("thread: make_listener failed for %s:%d (%s)", bind_ip, port, strerror(errno));
        g_running = 0;
        return 0;
    }

    NOX_CTRL_LOG("thread: listening on %s:%d (fd=%d)", bind_ip, port, g_server_fd);

    while (g_running) {
        struct sockaddr_storage ss;
        socklen_t sl = sizeof(ss);
        int cfd = (int)accept(g_server_fd, (struct sockaddr *)&ss, &sl);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            NOX_CTRL_LOG("thread: accept failed (%s)", strerror(errno));
            continue;
        }

        // Log peer
        char host[NI_MAXHOST], serv[NI_MAXSERV];
        if (getnameinfo((struct sockaddr*)&ss, sl, host, sizeof(host), serv, sizeof(serv),
                        NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
            NOX_CTRL_LOG("thread: client connected from %s:%s (fd=%d)", host, serv, cfd);
        } else {
            NOX_CTRL_LOG("thread: client connected (fd=%d)", cfd);
        }

        // one client at a time
        if (g_client_fd >= 0) {
            NOX_CTRL_LOG("thread: dropping previous client fd=%d", g_client_fd);
            close_fd(&g_client_fd);
        }
        g_client_fd = cfd;

        set_sock_timeouts(g_client_fd, 2000);

        int authed = 0;
        send_str(g_client_fd, "NOX control server. Type 'help'.\r\n");
        send_str(g_client_fd, "auth <password>\r\n> ");

        char line[1024];
        for (;;) {
            int rl = recv_line_telnet(g_client_fd, line, sizeof(line));
            if (rl == 0) {
                NOX_CTRL_LOG("thread: client EOF");
                close_fd(&g_client_fd);
                break;
            }
            if (rl < 0) {
                // Usually timeout; just continue waiting
                // But if the socket is dead, bail.
                if (errno == ECONNRESET || errno == EPIPE) {
                    NOX_CTRL_LOG("thread: client disconnected (%s)", strerror(errno));
                    close_fd(&g_client_fd);
                    break;
                }
                continue;
            }

            if (rl == 2) {
                // Ctrl+C / Telnet IP -> treat like Escape (or ignore)
                enqueue_key_press(SDL_SCANCODE_ESCAPE);

                // Optional feedback to the client:
                send_str(g_client_fd, "^C -> ESC\r\n> ");
                continue;
            }

            // trim
            char *s = line;
            while (*s && (unsigned char)*s <= ' ') s++;
            char *e = s + strlen(s);
            while (e > s && (unsigned char)e[-1] <= ' ') *--e = 0;

            NOX_CTRL_LOG("thread: recv line: '%s'", s);

            if (!*s) { send_str(g_client_fd, "> "); continue; }

            strip_hash_comment(s);

            char *cmds[32];
            int ncmd = split_commands(s, cmds, (int)(sizeof(cmds)/sizeof(cmds[0])));

            for (int i=0;i<ncmd;i++) {
                char *c = cmds[i];
                while (*c && (unsigned char)*c <= ' ') c++;
                char *ce = c + strlen(c);
                while (ce > c && (unsigned char)ce[-1] <= ' ') *--ce = 0;
                if (!*c) continue;

                NOX_CTRL_LOG("thread: cmd: '%s'", c);

                int before = g_client_fd;
                handle_one_command(g_client_fd, c, &authed, pw);
                if (before >= 0 && g_client_fd < 0) break;
            }

            if (g_client_fd < 0) break;
            send_str(g_client_fd, "\r\n> ");
        }
    }

    NOX_CTRL_LOG("thread: shutting down");
    close_fd(&g_client_fd);
    close_fd(&g_server_fd);
    g_running = 0;
    return 0;
}

// ------------------------------------------------------------
// Public API (called from win.c and input.c)
// ------------------------------------------------------------
void nox_control_server_init(void)
{
    const char *en = getenv("NOX_CONTROL_SERVER");
    const char *pw = getenv("NOX_CONTROL_SERVER_PASSWORD");
    const char *bind_ip = nox_env_str("NOX_CONTROL_SERVER_BIND", "127.0.0.1");
    int port = nox_env_int("NOX_CONTROL_SERVER_PORT", 2323);

    NOX_CTRL_LOG("init: NOX_CONTROL_SERVER='%s' NOX_CONTROL_SERVER_PASSWORD='%s' bind=%s port=%d",
                 en ? en : "(null)",
                 (pw && *pw) ? "(set)" : "(missing/empty)",
                 bind_ip, port);

    if (!nox_env_truthy(en)) {
        NOX_CTRL_LOG("init: disabled (NOX_CONTROL_SERVER not truthy)");
        return;
    }

    if (!pw || !*pw) {
        NOX_CTRL_LOG("init: refusing to start (NOX_CONTROL_SERVER_PASSWORD missing/empty)");
        return;
    }

    if (!g_qmu) g_qmu = SDL_CreateMutex();
    if (!g_qmu) {
        NOX_CTRL_LOG("init: SDL_CreateMutex failed: %s", SDL_GetError());
        return;
    }

    // Run local bootstrap commands once (does not require a client).
    nox_control_server_bootstrap_from_env();

    g_running = 1;
    g_thr = SDL_CreateThread(server_thread, "nox_ctrl", NULL);
    if (!g_thr) {
        NOX_CTRL_LOG("init: SDL_CreateThread failed: %s", SDL_GetError());
        g_running = 0;
        return;
    }

    NOX_CTRL_LOG("init: thread started OK");
}


void nox_control_server_pump(void)
 {
     // Called on main thread frequently.
     // IMPORTANT: must never block the game loop. So sleep is implemented as
     // "pause processing actions until SDL_GetTicks() reaches g_sleep_until".
     static Uint32 g_sleep_until = 0;

     Uint32 now = SDL_GetTicks();
     if (g_sleep_until && (Sint32)(now - g_sleep_until) < 0) {
         // Still sleeping: don't process any actions yet.
         return;
     }
     g_sleep_until = 0;
    // NEW: if we're in the middle of a TRHOME slam, finish it BEFORE processing queue
    if (g_trhome_left > 0) {
        nox_ctrl_inject_mouse_move(+16384, -16384, 0);
        g_trhome_left--;

        if (g_trhome_left > 0) {
            g_sleep_until = SDL_GetTicks() + 10;
        }
        return;
    }
     ControlAction a;
     while (q_pop(&a)) {
         switch (a.type) {
         case ACT_MOVE:
             nox_ctrl_inject_mouse_move(a.a, a.b, 0);
             break;

         case ACT_BTN:
             nox_ctrl_inject_mouse_button(a.a, a.down);
             break;

         case ACT_KEY:
             nox_ctrl_inject_key_scancode(a.a, a.down);
             break;

         case ACT_TEXT:
             nox_ctrl_inject_text_utf8(a.text);
             break;

         case ACT_SLEEP_MS: {
             // Non-blocking: defer remaining queued actions to future frames.
             int ms = a.a;
             if (ms < 0) ms = 0;
             if (ms > 60000) ms = 60000;
             NOX_CTRL_LOG("Sleep for %d ms", ms);
             g_sleep_until = SDL_GetTicks() + (Uint32)ms;
             return; // stop processing more actions this frame
         }

         case ACT_HOME:
             // slam to top-left using repeated large negative moves WITHOUT blocking
             // Spread it across frames using short non-blocking sleeps.
             for (int i = 0; i < 3; i++) {
                 nox_ctrl_inject_mouse_move(-16384, -16384, 0);
                 // schedule a tiny delay before continuing remaining actions
                 g_sleep_until = SDL_GetTicks() + 10;
                 return;
             }
             break;
        case ACT_TRHOME:
            // NEW: start a 3-step slam to top-right, non-blocking
            g_trhome_left = 3;
            // do first step immediately
            nox_ctrl_inject_mouse_move(+16384, -16384, 0);
            g_trhome_left--;
            if (g_trhome_left > 0) {
                g_sleep_until = SDL_GetTicks() + 10;
            }
            return;
        case ACT_QUIT_GAME:
        {
            SDL_Event ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = SDL_QUIT;
            SDL_PushEvent(&ev);
            return; // stop processing more actions this frame
        }
        case ACT_LOG:
            NOX_CTRL_LOG("%s", a.text);
            break;

//         case ACT_MACRO_START_SERVER:
//             enqueue_macro_start_server();
//             break;

         default:
             break;
         }
     }
 }




#endif // USE_SDL
