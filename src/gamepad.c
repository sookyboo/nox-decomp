// gamepad.c
#include "defs.h"

#ifdef USE_SDL
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* forward decl so NOX_GAMEPAD_LOG can call it without implicit-decl warnings */
static int g_gamepad_log_enabled(void);

#ifndef NOX_GAMEPAD_LOG
#define NOX_GAMEPAD_LOG(...) do { if (g_gamepad_log_enabled()) fprintf(stderr, __VA_ARGS__); } while (0)
#endif

// --------------------------
// External injectors (input.c)
// --------------------------
extern void nox_ctrl_inject_mouse_move(int dx, int dy, int wheel);
extern void nox_ctrl_inject_mouse_button(int button, int down);
extern void nox_ctrl_inject_key_scancode(int sdl_scancode, int down);
//extern void nox_ctrl_inject_text_utf8(const char *utf8);

// --------------------------
// Small portability helpers
// --------------------------
static char *nox_strdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char*)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

// --------------------------
// Env helpers
// --------------------------
static int nox_env_truthy_default(const char *name, int def_val)
{
    const char *s = getenv(name);
    if (!s) return def_val;

    while (*s && (unsigned char)*s <= ' ') s++;
    if (!*s) return def_val;

    // exact "0" disables
    if (s[0] == '0' && s[1] == 0) return 0;
    if (s[0] == '1' && s[1] == 0) return 1;

    char buf[16];
    size_t n = 0;
    while (s[n] && n + 1 < sizeof(buf)) {
        buf[n] = (char)tolower((unsigned char)s[n]);
        n++;
    }
    buf[n] = 0;

    if (!strcmp(buf, "true") || !strcmp(buf, "yes") || !strcmp(buf, "on")) return 1;
    if (!strcmp(buf, "false") || !strcmp(buf, "no") || !strcmp(buf, "off")) return 0;

    // Anything else: treat as truthy
    return 1;
}

static int nox_env_int_default(const char *name, int def_val)
{
    const char *s = getenv(name);
    if (!s || !*s) return def_val;
    return atoi(s);
}

// --------------------------
// Logging toggle
// --------------------------
static int g_gamepad_log_cached = -1;
static int g_gamepad_log_enabled(void)
{
    if (g_gamepad_log_cached < 0) {
        g_gamepad_log_cached = nox_env_truthy_default("NOX_GAMEPAD_LOG", 0) ? 1 : 0;
    }
    return g_gamepad_log_cached;
}

// --------------------------
// INI parsing utilities
// --------------------------
static char *trim(char *s)
{
    if (!s) return s;
    while (*s && (unsigned char)*s <= ' ') s++;
    char *e = s + strlen(s);
    while (e > s && (unsigned char)e[-1] <= ' ') e--;
    *e = 0;
    return s;
}

static void strip_comment(char *s)
{
    // remove ; or # comment (not quote-aware; matches your earlier assumption)
    for (char *p = s; p && *p; ++p) {
        if (*p == ';' || *p == '#') {
            *p = 0;
            return;
        }
    }
}

// tokenizes: wordset = cheats "a" "b c" "d"
static int tokenize_quoted(const char *in, char out[][256], int max)
{
    int n = 0;
    const char *p = in;
    while (*p && n < max) {
        while (*p && (unsigned char)*p <= ' ') p++;
        if (!*p) break;

        if (*p == '"') {
            p++;
            int k = 0;
            while (*p && *p != '"' && k < 255) out[n][k++] = *p++;
            out[n][k] = 0;
            if (*p == '"') p++;
            n++;
        } else {
            int k = 0;
            while (*p && (unsigned char)*p > ' ' && *p != '"' && k < 255) out[n][k++] = *p++;
            out[n][k] = 0;
            n++;
        }
    }
    return n;
}

// --------------------------
// Config + mappings
// --------------------------
enum overlay_mode { OVERLAY_PARENT = 0, OVERLAY_CLEAR = 1 };

enum phys_input {
    IN_A, IN_B, IN_X, IN_Y,
    IN_START, IN_SELECT, IN_GUIDE,
    IN_L1, IN_L2, IN_R1, IN_R2,
    IN_DPAD, IN_LEFT_ANALOG,
    IN_UP, IN_DOWN, IN_LEFT, IN_RIGHT,
    IN_RU, IN_RD, IN_RL, IN_RR,
    IN__COUNT
};

enum action_type {
    ACT_NONE = 0,
    ACT_MOUSE_MOVEMENT,
    ACT_MOUSE_LEFT,
    ACT_MOUSE_RIGHT,
    ACT_MOUSE_MIDDLE,
    ACT_MOUSE_SLOW,
    ACT_KEY,          // payload: scancode
    ACT_HOLD_STATE,   // payload: group index
    ACT_PREV_WORD,
    ACT_NEXT_WORD,
    ACT_FINISH_TEXT,
    ACT_CANCEL_TEXT
};

struct action {
    enum action_type type;
    int payload; // scancode or group index
};

#define MAX_WORDSETS   16
#define MAX_WORDS      128
#define MAX_LAYERS     16
#define MAX_GROUP_NAME 64

struct wordset {
    char name[64];
    char *words[MAX_WORDS];
    int word_count;

    int index;       // current selection
    int preview_len; // number of characters we typed (ASCII bytes)
};

struct layer {
    char name[MAX_GROUP_NAME]; // "" for base
    enum overlay_mode overlay;
    int wordset_idx; // -1 none
    struct action binds[IN__COUNT];
};

struct cfg {
    int repeat_delay_ms;
    int repeat_rate_hz;
    int mouse_delay_ms;
    int mouse_slow_scale;
    int deadzone_x, deadzone_y, deadzone_triggers;
    int dpad_mouse_normalize;
};

static struct cfg g_cfg;

// layers: [0] is base [controls]
static struct layer g_layers[MAX_LAYERS];
static int g_layer_count = 0;

// wordsets
static struct wordset g_wordsets[MAX_WORDSETS];
static int g_wordset_count = 0;

// --------------------------
// Active stack with explicit parent pointers (FIX)
// --------------------------
struct active_entry {
    int layer_idx;   // which layer
    int parent_slot; // slot index of the layer that activated this one (-1 for base)
};
static struct active_entry g_active_stack[MAX_LAYERS];
static int g_active_count = 0;

// --------------------------
// SDL controller state
// --------------------------
static SDL_GameController *g_gc = NULL;
static SDL_JoystickID g_joy_id = -1;

static const int AXIS_DIGITAL_THRESHOLD = 16000; // ~0.5

struct pad_snapshot {
    int a,b,x,y;
    int start,select,guide;
    int l1,l2_btn,r1,r2_btn; // triggers as digital with hysteresis
    int dpad_up,dpad_down,dpad_left,dpad_right;

    int lx, ly;
    int rx, ry;
    int lt, rt; // triggers (0..32767)
};

static struct pad_snapshot g_prev, g_cur;

static int g_enabled_cached = -1;
static int nox_gamepad_enabled(void)
{
    if (g_enabled_cached < 0) {
        g_enabled_cached = nox_env_truthy_default("NOX_GAMEPAD", 1) ? 1 : 0;
    }
    return g_enabled_cached;
}

static int g_exit_enabled_cached = -1;
static int nox_gamepad_exit_enabled(void)
{
    if (g_exit_enabled_cached < 0) {
        g_exit_enabled_cached = nox_env_truthy_default("NOX_GAMEPAD_EXIT", 0) ? 1 : 0;
    }
    return g_exit_enabled_cached;
}

static int g_autoswap_cached = -1;
static int nox_gamepad_autoswap_xbox(void)
{
    if (g_autoswap_cached < 0) {
        g_autoswap_cached = nox_env_truthy_default("NOX_GAMEPAD_AUTOSWAP_XBOX", 1) ? 1 : 0;
    }
    return g_autoswap_cached;
}

static int g_flip_cached = -1;
static int nox_gamepad_flip_abxy(void)
{
    if (g_flip_cached < 0) {
        g_flip_cached = nox_env_truthy_default("NOX_GAMEPAD_FLIP_ABXY", 0) ? 1 : 0;
    }
    return g_flip_cached;
}

static int str_icontains(const char *hay, const char *needle)
{
    if (!hay || !needle || !*needle) return 0;
    size_t nh = strlen(hay), nn = strlen(needle);
    for (size_t i = 0; i + nn <= nh; ++i) {
        size_t k = 0;
        while (k < nn) {
            char a = (char)tolower((unsigned char)hay[i+k]);
            char b = (char)tolower((unsigned char)needle[k]);
            if (a != b) break;
            k++;
        }
        if (k == nn) return 1;
    }
    return 0;
}

static int detect_xbox_style(SDL_GameController *gc)
{
    const char *name = SDL_GameControllerName(gc);
    if (!name) name = "";

    if (str_icontains(name, "xbox")) return 1;
    if (str_icontains(name, "microsoft")) return 1;
    if (str_icontains(name, "xinput")) return 1;
    if (str_icontains(name, "360")) return 1;
    if (str_icontains(name, "x-box")) return 1;

    return 0;
}

// Nintendo-assumed INI: if Xbox detected, swap A<->B and X<->Y unless overridden.
static int g_detected_xbox = 0;
static int g_effective_swap = 0;

static void compute_swap_policy(void)
{
    int auto_swap = (nox_gamepad_autoswap_xbox() && g_detected_xbox) ? 1 : 0;
    int toggle = nox_gamepad_flip_abxy() ? 1 : 0;
    g_effective_swap = auto_swap ^ toggle;

    NOX_GAMEPAD_LOG("[pad] name='%s' xbox=%d autoswap=%d flip=%d => swap=%d\n",
        g_gc ? (SDL_GameControllerName(g_gc) ? SDL_GameControllerName(g_gc) : "(null)") : "(no gc)",
        g_detected_xbox,
        nox_gamepad_autoswap_xbox(),
        nox_gamepad_flip_abxy(),
        g_effective_swap);

    if (g_gc) {
        const char *m = SDL_GameControllerMapping(g_gc);
        if (m) NOX_GAMEPAD_LOG("[pad] mapping=%s\n", m);
    }
}

// --------------------------
// Mapping helpers
// --------------------------
static void layer_init(struct layer *L, const char *name)
{
    memset(L, 0, sizeof(*L));
    if (name) strncpy(L->name, name, sizeof(L->name)-1);
    L->overlay = OVERLAY_PARENT;
    L->wordset_idx = -1;
    for (int i = 0; i < IN__COUNT; ++i) {
        L->binds[i].type = ACT_NONE;
        L->binds[i].payload = 0;
    }
}

static int find_layer(const char *name)
{
    for (int i = 0; i < g_layer_count; ++i) {
        if (!strcmp(g_layers[i].name, name)) return i;
    }
    return -1;
}

static int ensure_layer(const char *name)
{
    int idx = find_layer(name);
    if (idx >= 0) return idx;
    if (g_layer_count >= MAX_LAYERS) return -1;
    layer_init(&g_layers[g_layer_count], name);
    return g_layer_count++;
}

static int find_wordset(const char *name)
{
    for (int i = 0; i < g_wordset_count; ++i) {
        if (!strcmp(g_wordsets[i].name, name)) return i;
    }
    return -1;
}

static int ensure_wordset(const char *name)
{
    int idx = find_wordset(name);
    if (idx >= 0) return idx;
    if (g_wordset_count >= MAX_WORDSETS) return -1;

    memset(&g_wordsets[g_wordset_count], 0, sizeof(g_wordsets[g_wordset_count]));
    strncpy(g_wordsets[g_wordset_count].name, name, sizeof(g_wordsets[g_wordset_count].name)-1);
    g_wordsets[g_wordset_count].index = 0;
    g_wordsets[g_wordset_count].preview_len = 0;
    return g_wordset_count++;
}

static enum phys_input phys_from_key(const char *k)
{
    if (!strcmp(k, "a")) return IN_A;
    if (!strcmp(k, "b")) return IN_B;
    if (!strcmp(k, "x")) return IN_X;
    if (!strcmp(k, "y")) return IN_Y;
    if (!strcmp(k, "start")) return IN_START;
    if (!strcmp(k, "select")) return IN_SELECT;
    if (!strcmp(k, "guide")) return IN_GUIDE;
    if (!strcmp(k, "l1")) return IN_L1;
    if (!strcmp(k, "l2")) return IN_L2;
    if (!strcmp(k, "r1")) return IN_R1;
    if (!strcmp(k, "r2")) return IN_R2;
    if (!strcmp(k, "dpad")) return IN_DPAD;
    if (!strcmp(k, "left_analog")) return IN_LEFT_ANALOG;
    if (!strcmp(k, "up")) return IN_UP;
    if (!strcmp(k, "down")) return IN_DOWN;
    if (!strcmp(k, "left")) return IN_LEFT;
    if (!strcmp(k, "right")) return IN_RIGHT;
    if (!strcmp(k, "right_analog_up")) return IN_RU;
    if (!strcmp(k, "right_analog_down")) return IN_RD;
    if (!strcmp(k, "right_analog_left")) return IN_RL;
    if (!strcmp(k, "right_analog_right")) return IN_RR;
    return IN__COUNT;
}

static int scancode_from_name(const char *s)
{
    if (!s || !*s) return -1;

    // letters
    if (strlen(s) == 1 && s[0] >= 'a' && s[0] <= 'z') return SDL_SCANCODE_A + (s[0] - 'a');
    // digits
    if (strlen(s) == 1 && s[0] >= '0' && s[0] <= '9') return SDL_SCANCODE_0 + (s[0] - '0');

    if (!strcmp(s, "space")) return SDL_SCANCODE_SPACE;
    if (!strcmp(s, "esc") || !strcmp(s, "escape")) return SDL_SCANCODE_ESCAPE;
    if (!strcmp(s, "tab")) return SDL_SCANCODE_TAB;
    if (!strcmp(s, "shift")) return SDL_SCANCODE_LSHIFT;
    if (!strcmp(s, "insert")) return SDL_SCANCODE_INSERT;
    if (!strcmp(s, "delete")) return SDL_SCANCODE_DELETE;
    if (!strcmp(s, "backspace")) return SDL_SCANCODE_BACKSPACE;
    if (!strcmp(s, "enter") || !strcmp(s, "return")) return SDL_SCANCODE_RETURN;

    if (s[0] == 'f' && isdigit((unsigned char)s[1])) {
        int n = atoi(s + 1);
        if (n >= 1 && n <= 24) return SDL_SCANCODE_F1 + (n - 1);
    }

    return -1;
}

static struct action action_from_value(const char *v)
{
    struct action a;
    a.type = ACT_NONE;
    a.payload = 0;

    if (!v) return a;

    if (!strcmp(v, "mouse_movement")) { a.type = ACT_MOUSE_MOVEMENT; return a; }
    if (!strcmp(v, "mouse_left"))     { a.type = ACT_MOUSE_LEFT; return a; }
    if (!strcmp(v, "mouse_right"))    { a.type = ACT_MOUSE_RIGHT; return a; }
    if (!strcmp(v, "mouse_middle"))   { a.type = ACT_MOUSE_MIDDLE; return a; }
    if (!strcmp(v, "mouse_slow"))     { a.type = ACT_MOUSE_SLOW; return a; }

    if (!strcmp(v, "prev_word"))      { a.type = ACT_PREV_WORD; return a; }
    if (!strcmp(v, "next_word"))      { a.type = ACT_NEXT_WORD; return a; }
    if (!strcmp(v, "finish_text"))    { a.type = ACT_FINISH_TEXT; return a; }
    if (!strcmp(v, "cancel_text"))    { a.type = ACT_CANCEL_TEXT; return a; }

    if (!strncmp(v, "hold_state", 10)) {
        const char *p = v + 10;
        while (*p && (unsigned char)*p <= ' ') p++;
        if (*p) {
            char gname[MAX_GROUP_NAME];
            strncpy(gname, p, sizeof(gname)-1);
            gname[sizeof(gname)-1] = 0;
            int gi = ensure_layer(gname);
            if (gi >= 0) {
                a.type = ACT_HOLD_STATE;
                a.payload = gi;
            }
        }
        return a;
    }

    int sc = scancode_from_name(v);
    if (sc >= 0) {
        a.type = ACT_KEY;
        a.payload = sc;
    }

    return a;
}

// --------------------------
// Overlay resolution (FIX: explicit parent pointers)
// --------------------------
static int slot_find_layer(int layer_idx)
{
    for (int i = 0; i < g_active_count; ++i) {
        if (g_active_stack[i].layer_idx == layer_idx) return i;
    }
    return -1;
}

static struct action resolve_binding(enum phys_input in)
{
    struct action none = { ACT_NONE, 0 };

    for (int start = g_active_count - 1; start >= 0; --start) {
        int slot = start;

        while (slot >= 0) {
            int li = g_active_stack[slot].layer_idx;
            if (li < 0 || li >= g_layer_count) break;

            struct layer *L = &g_layers[li];
            struct action a = L->binds[in];
            if (a.type != ACT_NONE) return a;

            if (L->overlay == OVERLAY_CLEAR) {
                // IMPORTANT: clear blocks everything below this active layer
                return none;
            }

            slot = g_active_stack[slot].parent_slot;
        }
    }

    return none;
}

static int resolve_wordset_idx(void)
{
    for (int start = g_active_count - 1; start >= 0; --start) {
        int slot = start;

        while (slot >= 0) {
            int li = g_active_stack[slot].layer_idx;
            if (li < 0 || li >= g_layer_count) break;

            struct layer *L = &g_layers[li];
            if (L->wordset_idx >= 0) return L->wordset_idx;

            if (L->overlay == OVERLAY_CLEAR) {
                // clear blocks everything below
                return -1;
            }

            slot = g_active_stack[slot].parent_slot;
        }
    }
    return -1;
}


// --------------------------
// Wordset operations
// --------------------------
//static int utf8_count_codepoints(const char *s)
//{
//    if (!s) return 0;
//    int n = 0;
//    const unsigned char *p = (const unsigned char*)s;
//    while (*p) {
//        if ((*p & 0xC0) != 0x80) n++;
//        p++;
//    }
//    return n;
//}

static void inject_key_tap(int sc)
{
    nox_ctrl_inject_key_scancode(sc, 1);
    nox_ctrl_inject_key_scancode(sc, 0);
}

static void inject_backspaces(int n)
{
    for (int i = 0; i < n; ++i) inject_key_tap(SDL_SCANCODE_BACKSPACE);
}

static void inject_tap_with_shift(int sc)
{
    nox_ctrl_inject_key_scancode(SDL_SCANCODE_LSHIFT, 1);
    inject_key_tap(sc);
    nox_ctrl_inject_key_scancode(SDL_SCANCODE_LSHIFT, 0);
}

static void inject_type_via_keys(const char *text)
{
    if (!text) return;

    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        unsigned char c = *p;

        if (c == ' ') { inject_key_tap(SDL_SCANCODE_SPACE); continue; }

        if (c >= '0' && c <= '9') {
            int sc = (c == '0') ? SDL_SCANCODE_0 : (SDL_SCANCODE_1 + (c - '1'));
            inject_key_tap(sc);
            continue;
        }

        if (c >= 'a' && c <= 'z') { inject_key_tap(SDL_SCANCODE_A + (c - 'a')); continue; }

        if (c >= 'A' && c <= 'Z') { inject_tap_with_shift(SDL_SCANCODE_A + (c - 'A')); continue; }

        // punctuation (US layout assumptions)
        switch (c) {
            case '-': inject_key_tap(SDL_SCANCODE_MINUS); break;
            case '_': inject_tap_with_shift(SDL_SCANCODE_MINUS); break;
            case '=': inject_key_tap(SDL_SCANCODE_EQUALS); break;
            case '+': inject_tap_with_shift(SDL_SCANCODE_EQUALS); break;
            case '.': inject_key_tap(SDL_SCANCODE_PERIOD); break;
            case ',': inject_key_tap(SDL_SCANCODE_COMMA); break;
            case '/': inject_key_tap(SDL_SCANCODE_SLASH); break;
            case ';': inject_key_tap(SDL_SCANCODE_SEMICOLON); break;
            case ':': inject_tap_with_shift(SDL_SCANCODE_SEMICOLON); break;
            case '\'': inject_key_tap(SDL_SCANCODE_APOSTROPHE); break;
            case '"': inject_tap_with_shift(SDL_SCANCODE_APOSTROPHE); break;
            default:
                break;
        }
    }
}

static Uint32 g_wordset_next_allowed_ms = 0;

static int wordset_cycle_delay_ms(void)
{
    // tune at runtime; default 120ms feels good
    return nox_env_int_default("NOX_GAMEPAD_WORDSET_CYCLE_MS", 120);
}


static void wordset_cycle(int dir)
{
    Uint32 now = SDL_GetTicks();
    if (now < g_wordset_next_allowed_ms)
        return;

    g_wordset_next_allowed_ms = now + (Uint32)wordset_cycle_delay_ms();

    int wsi = resolve_wordset_idx();
    if (wsi < 0 || wsi >= g_wordset_count) return;

    struct wordset *ws = &g_wordsets[wsi];
    if (ws->word_count <= 0) return;

    if (ws->preview_len > 0) {
        inject_backspaces(ws->preview_len);
        ws->preview_len = 0;
    }

    ws->index += dir;
    if (ws->index < 0) ws->index = ws->word_count - 1;
    if (ws->index >= ws->word_count) ws->index = 0;

    const char *phrase = ws->words[ws->index] ? ws->words[ws->index] : "";
    if (*phrase) {
        inject_type_via_keys(phrase);              // <--- instead of nox_ctrl_inject_text_utf8
        ws->preview_len = (int)strlen(phrase);     // <--- now backspaces should count chars we typed
    }
}

static void wordset_cancel(void)
{
    int wsi = resolve_wordset_idx();
    if (wsi < 0 || wsi >= g_wordset_count) return;

    struct wordset *ws = &g_wordsets[wsi];
    if (ws->preview_len > 0) {
        inject_backspaces(ws->preview_len);
        ws->preview_len = 0;
    }
}

static void wordset_finish(void)
{
    inject_key_tap(SDL_SCANCODE_RETURN);

    int wsi = resolve_wordset_idx();
    if (wsi >= 0 && wsi < g_wordset_count) {
        g_wordsets[wsi].preview_len = 0;
    }
}

// --------------------------
// Repeat handling for next/prev
// --------------------------
struct repeat_state {
    int held;
    Uint32 next_ms;
};
static struct repeat_state g_rep_prev = {0,0};
static struct repeat_state g_rep_next = {0,0};

static int should_fire_repeat(struct repeat_state *rs, int is_down, Uint32 now_ms)
{
    if (!is_down) {
        rs->held = 0;
        rs->next_ms = 0;
        return 0;
    }

    if (!rs->held) {
        rs->held = 1;
        rs->next_ms = now_ms + (Uint32)g_cfg.repeat_delay_ms;
        return 1;
    }

    if (now_ms >= rs->next_ms) {
        Uint32 interval = (g_cfg.repeat_rate_hz > 0) ? (Uint32)(1000 / g_cfg.repeat_rate_hz) : 16;
        if (interval < 1) interval = 1;
        rs->next_ms = now_ms + interval;
        return 1;
    }
    return 0;
}

// --------------------------
// Mouse movement from analog/dpad
// --------------------------
static int apply_deadzone(int v, int dz)
{
    int av = (v < 0) ? -v : v;
    if (av <= dz) return 0;
    int sign = (v < 0) ? -1 : 1;
    return sign * (av - dz);
}

static int phys_is_down(enum phys_input in)
{
    switch (in) {
        case IN_A:      return g_cur.a;
        case IN_B:      return g_cur.b;
        case IN_X:      return g_cur.x;
        case IN_Y:      return g_cur.y;
        case IN_START:  return g_cur.start;
        case IN_SELECT: return g_cur.select;
        case IN_GUIDE:  return g_cur.guide;
        case IN_L1:     return g_cur.l1;
        case IN_R1:     return g_cur.r1;
        case IN_L2:     return g_cur.l2_btn;
        case IN_R2:     return g_cur.r2_btn;
        case IN_UP:     return g_cur.dpad_up;
        case IN_DOWN:   return g_cur.dpad_down;
        case IN_LEFT:   return g_cur.dpad_left;
        case IN_RIGHT:  return g_cur.dpad_right;
        case IN_RU:     return (g_cur.ry < -AXIS_DIGITAL_THRESHOLD);
        case IN_RD:     return (g_cur.ry >  AXIS_DIGITAL_THRESHOLD);
        case IN_RL:     return (g_cur.rx < -AXIS_DIGITAL_THRESHOLD);
        case IN_RR:     return (g_cur.rx >  AXIS_DIGITAL_THRESHOLD);
        default:        return 0;
    }
}

static int mouse_slow_active(void)
{
    for (int in = 0; in < IN__COUNT; ++in) {
        if (!phys_is_down((enum phys_input)in)) continue;

        struct action a = resolve_binding((enum phys_input)in);
        if (a.type == ACT_MOUSE_SLOW) return 1;
    }
    return 0;
}


static void do_mouse_movement(Uint32 now_ms)
{
    static Uint32 last_mouse_ms = 0;
    if (g_cfg.mouse_delay_ms > 0) {
        if (now_ms - last_mouse_ms < (Uint32)g_cfg.mouse_delay_ms) return;
    }
    last_mouse_ms = now_ms;

    struct action dpad_act = resolve_binding(IN_DPAD);
    struct action la_act   = resolve_binding(IN_LEFT_ANALOG);

    int dx = 0, dy = 0;

    if (dpad_act.type == ACT_MOUSE_MOVEMENT) {
        int mx = (g_cur.dpad_right ? 1 : 0) - (g_cur.dpad_left ? 1 : 0);
        int my = (g_cur.dpad_down  ? 1 : 0) - (g_cur.dpad_up   ? 1 : 0);

        if (mx || my) {
            double fx = (double)mx;
            double fy = (double)my;
            if (g_cfg.dpad_mouse_normalize && mx && my) {
                double inv = 1.0 / sqrt(2.0);
                fx *= inv;
                fy *= inv;
            }
            dx += (int)lrint(fx * 8.0);
            dy += (int)lrint(fy * 8.0);
        }
    }

    if (la_act.type == ACT_MOUSE_MOVEMENT) {
        int lx = apply_deadzone(g_cur.lx, g_cfg.deadzone_x);
        int ly = apply_deadzone(g_cur.ly, g_cfg.deadzone_y);

        dx += (int)lrint((double)lx / 4096.0);
        dy += (int)lrint((double)ly / 4096.0);
    }

    if (!dx && !dy) return;

    int slow = mouse_slow_active();

    if (slow && g_cfg.mouse_slow_scale > 0) {
        dx = (dx * g_cfg.mouse_slow_scale) / 100;
        dy = (dy * g_cfg.mouse_slow_scale) / 100;
        if (dx == 0 && dy == 0) return;
    }

    nox_ctrl_inject_mouse_move(dx, dy, 0);
}

// --------------------------
// Hold-state stack management (FIX: parent chain + safe removal)
// --------------------------
static int stack_contains_layer(int layer_idx)
{
    return slot_find_layer(layer_idx) >= 0;
}

static void stack_push_with_parent(int layer_idx, int parent_slot)
{
    if (layer_idx < 0 || layer_idx >= g_layer_count) return;
    if (stack_contains_layer(layer_idx)) return;
    if (g_active_count >= MAX_LAYERS) return;

    g_active_stack[g_active_count].layer_idx = layer_idx;
    g_active_stack[g_active_count].parent_slot = parent_slot;
    g_active_count++;

    NOX_GAMEPAD_LOG("[pad] push layer '%s' parent_slot=%d\n", g_layers[layer_idx].name, parent_slot);
}

static void stack_remove_slot_and_children(int slot_to_remove)
{
    if (slot_to_remove < 0 || slot_to_remove >= g_active_count) return;

    int remove[MAX_LAYERS];
    int map[MAX_LAYERS];
    memset(remove, 0, sizeof(remove));
    for (int i = 0; i < MAX_LAYERS; ++i) map[i] = -1;

    remove[slot_to_remove] = 1;

    // Mark any children (and grandchildren) whose parent chain includes a removed slot
    for (;;) {
        int changed = 0;
        for (int i = 0; i < g_active_count; ++i) {
            if (remove[i]) continue;
            int p = g_active_stack[i].parent_slot;
            if (p >= 0 && p < g_active_count && remove[p]) {
                remove[i] = 1;
                changed = 1;
            }
        }
        if (!changed) break;
    }

    // Build new stack, mapping old slot -> new slot
    struct active_entry newstk[MAX_LAYERS];
    int newcount = 0;
    for (int i = 0; i < g_active_count; ++i) {
        if (remove[i]) continue;
        map[i] = newcount;
        newstk[newcount++] = g_active_stack[i];
    }

    // Fix parent_slot indices in new stack
    for (int i = 0; i < newcount; ++i) {
        int old_parent = newstk[i].parent_slot;
        if (old_parent < 0) {
            newstk[i].parent_slot = -1;
        } else if (old_parent >= 0 && old_parent < g_active_count) {
            newstk[i].parent_slot = map[old_parent]; // may become -1 if parent removed
        } else {
            newstk[i].parent_slot = -1;
        }
    }

    // Log removals
    for (int i = g_active_count - 1; i >= 0; --i) {
        if (remove[i]) {
            int li = g_active_stack[i].layer_idx;
            if (li >= 0 && li < g_layer_count) {
                NOX_GAMEPAD_LOG("[pad] pop layer '%s'\n", g_layers[li].name);
            }
        }
    }

    /* copy only what we built (cleaner + avoids copying garbage) */
    memcpy(g_active_stack, newstk, (size_t)newcount * sizeof(newstk[0]));
    g_active_count = newcount;
}

static void stack_remove_layer(int layer_idx)
{
    int slot = slot_find_layer(layer_idx);
    if (slot < 0) return;
    stack_remove_slot_and_children(slot);
}

// --------------------------
// INI loading
// --------------------------
static FILE *open_cfg_file(char *out_path, size_t out_sz)
{
    const char *p = getenv("NOX_GAMEPAD_INI");
    if (p && *p) {
        FILE *f = fopen(p, "rb");
        if (f) {
            strncpy(out_path, p, out_sz-1);
            out_path[out_sz-1] = 0;
            return f;
        }
    }

    const char *conf = getenv("NOX_CONF_DIR");
    if (conf && *conf) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "%s/%s", conf, "gamepad.ini");
        FILE *f = fopen(buf, "rb");
        if (f) {
            strncpy(out_path, buf, out_sz-1);
            out_path[out_sz-1] = 0;
            return f;
        }
    }

    {
        const char *cands[] = { "gamepad.ini", "nox.gptk2.ini", "noxd.gptok2.ini", NULL };
        for (int i = 0; cands[i]; ++i) {
            FILE *f = fopen(cands[i], "rb");
            if (f) {
                strncpy(out_path, cands[i], out_sz-1);
                out_path[out_sz-1] = 0;
                return f;
            }
        }
    }

    return NULL;
}

static void free_wordsets(void)
{
    for (int i = 0; i < g_wordset_count; ++i) {
        for (int j = 0; j < g_wordsets[i].word_count; ++j) {
            free(g_wordsets[i].words[j]);
            g_wordsets[i].words[j] = NULL;
        }
        g_wordsets[i].word_count = 0;
        g_wordsets[i].index = 0;
        g_wordsets[i].preview_len = 0;
    }
    g_wordset_count = 0;
}

static void reset_mappings(void)
{
    free_wordsets();

    g_layer_count = 0;
    ensure_layer(""); // base layer

    g_layers[0].overlay = OVERLAY_PARENT;
    g_layers[0].wordset_idx = -1;

    memset(&g_cfg, 0, sizeof(g_cfg));
    g_cfg.repeat_delay_ms = 16;
    g_cfg.repeat_rate_hz = 60;
    g_cfg.mouse_delay_ms = 16;
    g_cfg.mouse_slow_scale = 40;
    g_cfg.deadzone_x = 2000;
    g_cfg.deadzone_y = 2000;
    g_cfg.deadzone_triggers = 3000;
    g_cfg.dpad_mouse_normalize = 1;
}

static void parse_wordset_line(const char *rhs)
{
    char toks[1 + MAX_WORDS][256];
    int n = tokenize_quoted(rhs, toks, 1 + MAX_WORDS);
    if (n < 1) return;

    int wsi = ensure_wordset(toks[0]);
    if (wsi < 0) return;

    struct wordset *ws = &g_wordsets[wsi];
    for (int i = 1; i < n && ws->word_count < MAX_WORDS; ++i) {
        if (!toks[i][0]) continue;
        ws->words[ws->word_count++] = nox_strdup(toks[i]);
    }
}

static void ini_load(void)
{
    reset_mappings();

    char path[1024] = {0};
    FILE *f = open_cfg_file(path, sizeof(path));
    if (!f) {
        NOX_GAMEPAD_LOG("[pad] no ini found (set NOX_GAMEPAD_INI or put gamepad.ini in NOX_CONF_DIR)\n");
        return;
    }

    NOX_GAMEPAD_LOG("[pad] loading ini: %s\n", path);

    char line[2048];
    int cur_layer = 0; // base
    int in_config = 0;

    while (fgets(line, sizeof(line), f)) {
        line[sizeof(line)-1] = 0;
        char *s = line;
        for (char *p = s; *p; ++p) if (*p == '\r' || *p == '\n') { *p = 0; break; }

        strip_comment(s);
        s = trim(s);
        if (!*s) continue;

        if (*s == '[') {
            char *e = strchr(s, ']');
            if (!e) continue;
            *e = 0;
            const char *sec = s + 1;

            in_config = 0;

            if (!strcmp(sec, "config")) {
                in_config = 1;
                cur_layer = 0;
                continue;
            }

            if (!strcmp(sec, "controls")) {
                cur_layer = 0;
                continue;
            }

            const char *prefix = "controls:";
            if (!strncmp(sec, prefix, strlen(prefix))) {
                const char *gname = sec + (int)strlen(prefix);
                cur_layer = ensure_layer(gname);
                continue;
            }

            cur_layer = 0;
            continue;
        }

        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = 0;
        char *k = trim(s);
        char *v = trim(eq + 1);

        if (*v == '"' && strlen(v) >= 2 && v[strlen(v)-1] == '"') {
            v[strlen(v)-1] = 0;
            v = trim(v + 1);
        }

        if (in_config) {
            if (!strcmp(k, "repeat_delay")) g_cfg.repeat_delay_ms = atoi(v);
            else if (!strcmp(k, "repeat_rate")) g_cfg.repeat_rate_hz = atoi(v);
            else if (!strcmp(k, "mouse_delay")) g_cfg.mouse_delay_ms = atoi(v);
            else if (!strcmp(k, "mouse_slow_scale")) g_cfg.mouse_slow_scale = atoi(v);
            else if (!strcmp(k, "deadzone_x")) g_cfg.deadzone_x = atoi(v);
            else if (!strcmp(k, "deadzone_y")) g_cfg.deadzone_y = atoi(v);
            else if (!strcmp(k, "deadzone_triggers")) g_cfg.deadzone_triggers = atoi(v);
            else if (!strcmp(k, "dpad_mouse_normalize")) {
                g_cfg.dpad_mouse_normalize = (!strcmp(v, "true") || !strcmp(v, "1") || !strcmp(v, "yes") || !strcmp(v, "on")) ? 1 : 0;
            }
            else if (!strcmp(k, "wordset")) {
                parse_wordset_line(v);
            }
            continue;
        }

        if (!strcmp(k, "overlay")) {
            g_layers[cur_layer].overlay = (!strcmp(v, "clear")) ? OVERLAY_CLEAR : OVERLAY_PARENT;
            continue;
        }

        if (!strcmp(k, "wordset")) {
            int wsi = find_wordset(v);
            if (wsi < 0) wsi = ensure_wordset(v);
            g_layers[cur_layer].wordset_idx = wsi;
            continue;
        }

        enum phys_input pin = phys_from_key(k);
        if (pin == IN__COUNT) continue;

        g_layers[cur_layer].binds[pin] = action_from_value(v);
    }

    fclose(f);
}

// --------------------------
// Controller open/close
// --------------------------

// Hysteresis state for trigger-as-button
static int g_l2_state = 0;
static int g_r2_state = 0;

static void nox_gamepad_open_if_needed(void)
{
    if (!nox_gamepad_enabled()) return;
    if (g_gc) return;

    int n = SDL_NumJoysticks();
    for (int i = 0; i < n; ++i) {
        if (!SDL_IsGameController(i)) continue;

        SDL_GameController *gc = SDL_GameControllerOpen(i);
        if (!gc) continue;

        g_gc = gc;
        SDL_Joystick *joy = SDL_GameControllerGetJoystick(gc);
        g_joy_id = joy ? SDL_JoystickInstanceID(joy) : -1;

        g_detected_xbox = detect_xbox_style(gc) ? 1 : 0;
        compute_swap_policy();

        memset(&g_prev, 0, sizeof(g_prev));
        memset(&g_cur, 0, sizeof(g_cur));

        /* FIX: reset trigger hysteresis when (re)opening controller */
        g_l2_state = 0;
        g_r2_state = 0;

        ini_load();

        // Base layer active, with no parent.
        g_active_count = 0;
        g_active_stack[g_active_count].layer_idx = 0;
        g_active_stack[g_active_count].parent_slot = -1;
        g_active_count = 1;

        NOX_GAMEPAD_LOG("[pad] opened controller idx=%d id=%d\n", i, (int)g_joy_id);
        return;
    }
}

void nox_gamepad_shutdown(void)
{
    if (g_gc) {
        NOX_GAMEPAD_LOG("[pad] closing controller\n");
        SDL_GameControllerClose(g_gc);
        g_gc = NULL;
        g_joy_id = -1;
    }

    free_wordsets();
    g_layer_count = 0;
    g_active_count = 0;

    /* also clear hysteresis so a later open starts clean */
    g_l2_state = 0;
    g_r2_state = 0;
}

// --------------------------
// Snapshot polling (FIX: trigger hysteresis)
// --------------------------
static int get_button(SDL_GameControllerButton b)
{
    return (g_gc && SDL_GameControllerGetButton(g_gc, b)) ? 1 : 0;
}

static int get_axis(SDL_GameControllerAxis a)
{
    if (!g_gc) return 0;
    return (int)SDL_GameControllerGetAxis(g_gc, a);
}

static int hysteresis_button(int value, int *state, int press_th, int release_th)
{
    if (!*state) {
        if (value >= press_th) *state = 1;
    } else {
        if (value <= release_th) *state = 0;
    }
    return *state;
}

static void poll_snapshot(struct pad_snapshot *out)
{
    memset(out, 0, sizeof(*out));
    if (!g_gc) return;

    int A = get_button(SDL_CONTROLLER_BUTTON_A);
    int B = get_button(SDL_CONTROLLER_BUTTON_B);
    int X = get_button(SDL_CONTROLLER_BUTTON_X);
    int Y = get_button(SDL_CONTROLLER_BUTTON_Y);

    if (g_effective_swap) {
        int t;
        t=A; A=B; B=t;
        t=X; X=Y; Y=t;
    }

    out->a = A;
    out->b = B;
    out->x = X;
    out->y = Y;

    out->start  = get_button(SDL_CONTROLLER_BUTTON_START);
    out->select = get_button(SDL_CONTROLLER_BUTTON_BACK);
    out->guide  = get_button(SDL_CONTROLLER_BUTTON_GUIDE);

    out->l1 = get_button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
    out->r1 = get_button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);

    int lt = get_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    int rt = get_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    if (lt < 0) lt = 0;
    if (rt < 0) rt = 0;
    out->lt = lt;
    out->rt = rt;

    // Hysteresis: press at deadzone_triggers, release at 2/3 of that.
    int press_th = g_cfg.deadzone_triggers;
    int release_th = (press_th * 2) / 3;
    if (release_th < 0) release_th = 0;

    out->l2_btn = hysteresis_button(lt, &g_l2_state, press_th, release_th);
    out->r2_btn = hysteresis_button(rt, &g_r2_state, press_th, release_th);

    out->dpad_up    = get_button(SDL_CONTROLLER_BUTTON_DPAD_UP);
    out->dpad_down  = get_button(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    out->dpad_left  = get_button(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    out->dpad_right = get_button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);

    out->lx = get_axis(SDL_CONTROLLER_AXIS_LEFTX);
    out->ly = get_axis(SDL_CONTROLLER_AXIS_LEFTY);
    out->rx = get_axis(SDL_CONTROLLER_AXIS_RIGHTX);
    out->ry = get_axis(SDL_CONTROLLER_AXIS_RIGHTY);
}

static int edge_down(int cur, int prev) { return (cur && !prev); }
static int edge_up(int cur, int prev)   { return (!cur && prev); }

// --------------------------
// Action execution
// --------------------------
static void exec_action_edge(struct action a, int down)
{
    switch (a.type) {
        case ACT_MOUSE_LEFT:   nox_ctrl_inject_mouse_button(0, down); break;
        case ACT_MOUSE_RIGHT:  nox_ctrl_inject_mouse_button(1, down); break;
        case ACT_MOUSE_MIDDLE: nox_ctrl_inject_mouse_button(2, down); break;
        case ACT_KEY:
            nox_ctrl_inject_key_scancode(a.payload, down);
            break;
        default:
            break;
    }
}

static void exec_action_press(struct action a, Uint32 now_ms)
{
    (void)now_ms;
    switch (a.type) {
        case ACT_PREV_WORD:    wordset_cycle(-1); break;
        case ACT_NEXT_WORD:    wordset_cycle(+1); break;
        case ACT_FINISH_TEXT:  wordset_finish(); break;
        case ACT_CANCEL_TEXT:  wordset_cancel(); break;
        default:
            break;
    }
}

// --------------------------
// Exit combo
// --------------------------
static int g_exit_latched = 0;
static void maybe_exit_combo(void)
{
    if (!nox_gamepad_exit_enabled()) return;

    if (!g_cur.start && !g_cur.select) {
        g_exit_latched = 0;
        return;
    }

    int start_down  = edge_down(g_cur.start,  g_prev.start);
    int select_down = edge_down(g_cur.select, g_prev.select);

    if (!g_exit_latched && ((start_down && g_cur.select) || (select_down && g_cur.start))) {
        g_exit_latched = 1;
        SDL_Event ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = SDL_QUIT;
        SDL_PushEvent(&ev);
        NOX_GAMEPAD_LOG("[pad] exit combo start+select => SDL_QUIT\n");
    }
}

// --------------------------
// Main update
// --------------------------
void nox_gamepad_update(void)
{
//    static int once = 0;
//    if (!once) {
//        once = 1;
//        fprintf(stderr,
//            "[pad] update() entered. NOX_GAMEPAD=%s NOX_GAMEPAD_LOG=%s\n",
//            getenv("NOX_GAMEPAD") ? getenv("NOX_GAMEPAD") : "(null)",
//            getenv("NOX_GAMEPAD_LOG") ? getenv("NOX_GAMEPAD_LOG") : "(null)");
//        fflush(stderr);
//    }
    if (!nox_gamepad_enabled()) return;

    nox_gamepad_open_if_needed();
    if (!g_gc) return;

    g_prev = g_cur;
    poll_snapshot(&g_cur);

    maybe_exit_combo();

    // ---- MOVIE SKIP REQUEST ----
        if (edge_down(g_cur.select, g_prev.select)) {
            g_movie_skip_requested = 1;

//            // Optional: also inject ESC so the movie loop exits immediately
//            SDL_Event ev;
//            memset(&ev, 0, sizeof(ev));
//            ev.type = SDL_KEYDOWN;
//            ev.key.keysym.sym = SDLK_ESCAPE;
//            SDL_PushEvent(&ev);
        }
        // ---- end movie skip request ----

    struct {
        enum phys_input in;
        int cur;
        int prev;
    } buttons[] = {
        { IN_A, g_cur.a, g_prev.a },
        { IN_B, g_cur.b, g_prev.b },
        { IN_X, g_cur.x, g_prev.x },
        { IN_Y, g_cur.y, g_prev.y },
        { IN_START, g_cur.start, g_prev.start },
        { IN_SELECT, g_cur.select, g_prev.select },
        { IN_GUIDE, g_cur.guide, g_prev.guide },
        { IN_L1, g_cur.l1, g_prev.l1 },
        { IN_L2, g_cur.l2_btn, g_prev.l2_btn },
        { IN_R1, g_cur.r1, g_prev.r1 },
        { IN_R2, g_cur.r2_btn, g_prev.r2_btn },
        { IN_UP, g_cur.dpad_up, g_prev.dpad_up },
        { IN_DOWN, g_cur.dpad_down, g_prev.dpad_down },
        { IN_LEFT, g_cur.dpad_left, g_prev.dpad_left },
        { IN_RIGHT, g_cur.dpad_right, g_prev.dpad_right },
        { IN_RU, (g_cur.ry < -AXIS_DIGITAL_THRESHOLD), (g_prev.ry < -AXIS_DIGITAL_THRESHOLD) },
        { IN_RD, (g_cur.ry >  AXIS_DIGITAL_THRESHOLD), (g_prev.ry >  AXIS_DIGITAL_THRESHOLD) },
        { IN_RL, (g_cur.rx < -AXIS_DIGITAL_THRESHOLD), (g_prev.rx < -AXIS_DIGITAL_THRESHOLD) },
        { IN_RR, (g_cur.rx >  AXIS_DIGITAL_THRESHOLD), (g_prev.rx >  AXIS_DIGITAL_THRESHOLD) },
    };

    Uint32 now_ms = SDL_GetTicks();

    // First: process hold_state down/up so stack is correct
    for (unsigned i = 0; i < sizeof(buttons)/sizeof(buttons[0]); ++i) {
        int down_edge = edge_down(buttons[i].cur, buttons[i].prev);
        int up_edge   = edge_up(buttons[i].cur, buttons[i].prev);
        if (!down_edge && !up_edge) continue;

        struct action a = resolve_binding(buttons[i].in);
        if (a.type == ACT_HOLD_STATE) {
            if (down_edge) {
                // Parent is the currently top active slot at activation time
                int parent_slot = (g_active_count > 0) ? (g_active_count - 1) : -1;
                stack_push_with_parent(a.payload, parent_slot);
            }
            if (up_edge) {
                stack_remove_layer(a.payload);
            }
        }
    }

    // Mouse movement from left analog / dpad
    do_mouse_movement(now_ms);

    // Now: process normal edge actions
    for (unsigned i = 0; i < sizeof(buttons)/sizeof(buttons[0]); ++i) {
        int down_edge = edge_down(buttons[i].cur, buttons[i].prev);
        int up_edge   = edge_up(buttons[i].cur, buttons[i].prev);
        if (!down_edge && !up_edge) continue;

        struct action a = resolve_binding(buttons[i].in);

        if (a.type == ACT_HOLD_STATE) continue;

        if (a.type == ACT_PREV_WORD || a.type == ACT_NEXT_WORD || a.type == ACT_FINISH_TEXT || a.type == ACT_CANCEL_TEXT) {
            if (down_edge) exec_action_press(a, now_ms);
        } else {
            if (down_edge) exec_action_edge(a, 1);
            if (up_edge)   exec_action_edge(a, 0);
        }
    }

    // Repeat prev/next when held for right stick left/right (as per your sample INI)
    {
        int left_held  = (g_cur.rx < -AXIS_DIGITAL_THRESHOLD);
        int right_held = (g_cur.rx >  AXIS_DIGITAL_THRESHOLD);

        struct action al = resolve_binding(IN_RL);
        struct action ar = resolve_binding(IN_RR);

        if (al.type == ACT_PREV_WORD) {
            if (should_fire_repeat(&g_rep_prev, left_held, now_ms)) wordset_cycle(-1);
        } else {
            g_rep_prev.held = 0;
        }

        if (ar.type == ACT_NEXT_WORD) {
            if (should_fire_repeat(&g_rep_next, right_held, now_ms)) wordset_cycle(+1);
        } else {
            g_rep_next.held = 0;
        }
    }
}

#endif // USE_SDL
