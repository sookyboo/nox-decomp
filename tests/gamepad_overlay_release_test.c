/* Regression for 64e57c7: a clear overlay must block base bindings while it
 * is held, then reveal them again after release. */
#include "../src/gamepad.c"

static int pad_buttons[SDL_CONTROLLER_BUTTON_MAX];
static int pad_axes[SDL_CONTROLLER_AXIS_MAX];
static int captured_scancode_count;

#ifdef _WIN32
static void set_gamepad_env(void)
{
    _putenv_s("NOX_GAMEPAD", "1");
}
#else
static void set_gamepad_env(void)
{
    setenv("NOX_GAMEPAD", "1", 1);
}
#endif

volatile int g_movie_skip_requested;

FILE *compat_fopen(const char *path, const char *mode)
{
    (void)path;
    (void)mode;
    return NULL;
}

char *compat_fgets(char *str, int size, FILE *stream)
{
    return fgets(str, size, stream);
}

Uint32 SDL_GetTicks(void) { return 100; }

Uint8 SDL_GameControllerGetButton(SDL_GameController *game_controller,
                                  SDL_GameControllerButton button)
{
    (void)game_controller;
    return (button >= 0 && button < SDL_CONTROLLER_BUTTON_MAX) ?
        (Uint8)pad_buttons[button] : 0;
}

Sint16 SDL_GameControllerGetAxis(SDL_GameController *game_controller,
                                 SDL_GameControllerAxis axis)
{
    (void)game_controller;
    return (axis >= 0 && axis < SDL_CONTROLLER_AXIS_MAX) ?
        (Sint16)pad_axes[axis] : 0;
}

void nox_ctrl_inject_key_scancode(int sdl_scancode, int down)
{
    (void)sdl_scancode;
    if (down) captured_scancode_count++;
}

void nox_ctrl_inject_mouse_move(int dx, int dy, int wheel)
{
    (void)dx; (void)dy; (void)wheel;
}

void nox_ctrl_inject_mouse_button(int button, int down)
{
    (void)button; (void)down;
}

void nox_ctrl_set_thumbstick_move(int active, int orientation, int move_cmd)
{
    (void)active; (void)orientation; (void)move_cmd;
}

void nox_ctrl_set_thumbstick_jump(int jump) { (void)jump; }

static void reset_fixture(void)
{
    memset(g_layers, 0, sizeof(g_layers));
    memset(g_active_stack, 0, sizeof(g_active_stack));
    memset(g_hold_layer_for_in, 0xff, sizeof(g_hold_layer_for_in));
    memset(&g_prev, 0, sizeof(g_prev));
    memset(&g_cur, 0, sizeof(g_cur));
    memset(&g_rs_prev, 0, sizeof(g_rs_prev));
    memset(&g_rs_cur, 0, sizeof(g_rs_cur));
    memset(pad_buttons, 0, sizeof(pad_buttons));
    memset(pad_axes, 0, sizeof(pad_axes));

    g_layer_count = 2;
    g_active_count = 1; /* base layer is always active during an update */
    g_active_stack[0].layer_idx = 0;
    g_active_stack[0].parent_slot = -1;
    g_set_layer_idx = -1;
    g_gc = (SDL_GameController *)(uintptr_t)1;
    g_effective_swap = 0;
    g_cfg.mouse_delay_ms = 0;
    g_cfg.deadzone_x = 2000;
    g_cfg.deadzone_y = 2000;
    g_cfg.deadzone_triggers = 3000;
    captured_scancode_count = 0;
    set_gamepad_env();

    /* L1 holds a clear overlay; A is only bound in the base layer. */
    g_layers[0].binds[IN_L1] = (struct action){ ACT_HOLD_STATE, 1 };
    g_layers[0].binds[IN_A] = (struct action){ ACT_KEY, SDL_SCANCODE_A };
    g_layers[1].overlay = OVERLAY_CLEAR;
}

int main(void)
{
    reset_fixture();

    /* Press L1 and A in one production update. The clear overlay is applied
     * before normal edge actions, so A must not reach the base key binding. */
    pad_buttons[SDL_CONTROLLER_BUTTON_LEFTSHOULDER] = 1;
    pad_buttons[SDL_CONTROLLER_BUTTON_A] = 1;
    nox_gamepad_update();
    if (g_active_count != 2 || captured_scancode_count != 0)
        return 1;

    /* Release the overlay, then press A again: the base binding is visible. */
    pad_buttons[SDL_CONTROLLER_BUTTON_LEFTSHOULDER] = 0;
    pad_buttons[SDL_CONTROLLER_BUTTON_A] = 0;
    nox_gamepad_update();
    if (g_active_count != 1)
        return 2;

    pad_buttons[SDL_CONTROLLER_BUTTON_A] = 1;
    nox_gamepad_update();
    if (captured_scancode_count != 1)
        return 3;

    return 0;
}
