/* Regression for 43df06e: slow mouse movement scales deltas by the configured
 * percentage instead of dividing by it. */
#include "../src/gamepad.c"

static int captured_dx;
static int captured_dy;
static int captured_count;

void nox_ctrl_inject_mouse_move(int dx, int dy, int wheel)
{
    (void)wheel;
    captured_dx = dx;
    captured_dy = dy;
    captured_count++;
}

static void reset_fixture(void)
{
    memset(&g_cfg, 0, sizeof(g_cfg));
    memset(g_layers, 0, sizeof(g_layers));
    memset(&g_cur, 0, sizeof(g_cur));
    memset(g_hold_layer_for_in, 0xff, sizeof(g_hold_layer_for_in));
    memset(g_active_stack, 0, sizeof(g_active_stack));

    g_layer_count = 1;
    g_set_layer_idx = -1;
    g_active_count = 0;
    g_cfg.mouse_delay_ms = 0;
    g_cfg.deadzone_x = 0;
    g_cfg.deadzone_y = 0;
    g_layers[0].binds[IN_LEFT_ANALOG] = (struct action){ ACT_MOUSE_MOVEMENT, 0 };
    g_layers[0].binds[IN_L2] = (struct action){ ACT_MOUSE_SLOW, 0 };

    /* These inputs produce an 8,-4 logical movement before slow scaling. */
    g_cur.lx = 32768;
    g_cur.ly = -16384;
    g_cur.l2_btn = 1;
    captured_dx = 0;
    captured_dy = 0;
    captured_count = 0;
}

static int expect_scale(int scale, int expected_dx, int expected_dy)
{
    setenv("NOX_GAMEPAD_MOUSE_BASE", "100", 1);
    g_cfg.mouse_slow_scale = scale;
    captured_count = 0;

    do_mouse_movement(1);

    return captured_count == 1 && captured_dx == expected_dx && captured_dy == expected_dy;
}

int main(void)
{
    reset_fixture();

    if (!expect_scale(25, 2, -1))
        return 1;
    if (!expect_scale(40, 3, -1))
        return 2;
    if (!expect_scale(100, 8, -4))
        return 3;
    if (!expect_scale(125, 10, -5))
        return 4;

    return 0;
}
