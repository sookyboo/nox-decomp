/* Regression for 8218c87: wordset cycling must replace the preview with
 * keyboard events, including shifted ASCII characters, and respect repeat
 * throttling. */
#include "../src/gamepad.c"

static Uint32 test_ticks;
static int captured_scancodes[64];
static int captured_down[64];
static int captured_count;

Uint32 SDL_GetTicks(void)
{
    return test_ticks;
}

void nox_ctrl_inject_key_scancode(int sdl_scancode, int down)
{
    if (captured_count < (int)(sizeof(captured_scancodes) / sizeof(captured_scancodes[0]))) {
        captured_scancodes[captured_count] = sdl_scancode;
        captured_down[captured_count] = down;
        captured_count++;
    }
}

static void reset_fixture(void)
{
    memset(g_wordsets, 0, sizeof(g_wordsets));
    memset(g_layers, 0, sizeof(g_layers));
    memset(g_active_stack, 0, sizeof(g_active_stack));
    g_wordset_count = 1;
    g_wordsets[0].words[0] = "abc";
    g_wordsets[0].words[1] = "A1+";
    g_wordsets[0].word_count = 2;
    g_wordsets[0].index = 0;
    g_layers[0].wordset_idx = 0;
    g_layer_count = 1;
    g_set_layer_idx = -1;
    g_active_count = 0;
    g_wordset_next_allowed_ms = 0;
    test_ticks = 100;
    captured_count = 0;
}

static int expect_event(int index, int scancode, int down)
{
    return index < captured_count &&
           captured_scancodes[index] == scancode &&
           captured_down[index] == down;
}

int main(void)
{
    reset_fixture();

    /* The first cycle types uppercase, numeric, and shifted punctuation. */
    wordset_cycle(+1);
    if (g_wordsets[0].index != 1 || g_wordsets[0].preview_len != 3)
        return 1;
    if (captured_count != 10 ||
        !expect_event(0, SDL_SCANCODE_LSHIFT, 1) ||
        !expect_event(1, SDL_SCANCODE_A, 1) ||
        !expect_event(2, SDL_SCANCODE_A, 0) ||
        !expect_event(3, SDL_SCANCODE_LSHIFT, 0) ||
        !expect_event(4, SDL_SCANCODE_1, 1) ||
        !expect_event(5, SDL_SCANCODE_1, 0) ||
        !expect_event(6, SDL_SCANCODE_LSHIFT, 1) ||
        !expect_event(7, SDL_SCANCODE_EQUALS, 1) ||
        !expect_event(8, SDL_SCANCODE_EQUALS, 0) ||
        !expect_event(9, SDL_SCANCODE_LSHIFT, 0))
        return 1;

    /* Repeated controller input inside the delay must not replace the text. */
    test_ticks = 110;
    wordset_cycle(+1);
    if (g_wordsets[0].index != 1 || captured_count != 10)
        return 1;

    /* At the boundary, the old preview is erased before the wrapped phrase. */
    test_ticks = 220;
    wordset_cycle(+1);
    if (g_wordsets[0].index != 0 || g_wordsets[0].preview_len != 3 || captured_count != 22)
        return 1;
    for (int i = 10; i < 16; ++i) {
        if (!expect_event(i, SDL_SCANCODE_BACKSPACE, (i - 10) % 2 == 0))
            return 1;
    }
    if (!expect_event(16, SDL_SCANCODE_A, 1) || !expect_event(17, SDL_SCANCODE_A, 0) ||
        !expect_event(18, SDL_SCANCODE_B, 1) || !expect_event(19, SDL_SCANCODE_B, 0) ||
        !expect_event(20, SDL_SCANCODE_C, 1) || !expect_event(21, SDL_SCANCODE_C, 0))
        return 1;

    return 0;
}
