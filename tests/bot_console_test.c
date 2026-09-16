#include "../src/bot_console.h"
#include "../src/bot_policy.h"
#include "../src/bot_runtime.h"
#include "../src/bot_trace.h"

#include <stdio.h>
#include <wchar.h>

int _nox_wcsicmp(const wchar_t *a, const wchar_t *b)
{
    while (*a && *b) {
        wchar_t ca = *a >= L'A' && *a <= L'Z' ? *a - L'A' + L'a' : *a;
        wchar_t cb = *b >= L'A' && *b <= L'Z' ? *b - L'A' + L'a' : *b;
        if (ca != cb)
            return ca < cb ? -1 : 1;
        ++a;
        ++b;
    }
    return *a == *b ? 0 : (*a ? 1 : -1);
}

static int g_server = 1;
static int g_object = 0x1234;
static int g_attach_calls;
static int g_detach_calls;
static int g_difficulty_calls;
static int g_spawn_calls;
static int g_spawn_fail_call;
static int g_clear_slot_calls;
static int g_cleared_slots[6];
static int g_clear_all_calls;
static int g_last_object;
static int g_last_slot;
static int g_last_class;
static nox_bot_spawn_team g_last_team;
static nox_bot_difficulty g_last_difficulty;

int sub_40A5C0(int flag)
{
    return flag == 1 ? g_server : 0;
}

int sub_450C00(unsigned char type, wchar_t *text, ...)
{
    (void)type;
    (void)text;
    return 1;
}

int nox_bot_engine_player_object_by_slot(int player_slot)
{
    return player_slot == 3 ? g_object : 0;
}

int nox_bot_runtime_attach_existing_player(int object, nox_bot_difficulty difficulty)
{
    ++g_attach_calls;
    g_last_object = object;
    g_last_difficulty = difficulty;
    return object == g_object;
}

int nox_bot_runtime_detach_existing_player(int object)
{
    ++g_detach_calls;
    g_last_object = object;
    return object == g_object;
}

int nox_bot_runtime_set_difficulty(int object, nox_bot_difficulty difficulty)
{
    ++g_difficulty_calls;
    g_last_object = object;
    g_last_difficulty = difficulty;
    return object == g_object;
}

int nox_bot_runtime_spawn_attempt(
    nox_bot_spawn_team team, int player_class, nox_bot_difficulty difficulty, int *spawned_slot)
{
    ++g_spawn_calls;
    g_last_team = team;
    g_last_class = player_class;
    g_last_difficulty = difficulty;
    if (g_spawn_fail_call && g_spawn_calls == g_spawn_fail_call)
        return 0;
    if (spawned_slot)
        *spawned_slot = 10 + g_spawn_calls;
    return 1;
}

int nox_bot_runtime_clear_server_created(int slot)
{
    if (g_clear_slot_calls < 6)
        g_cleared_slots[g_clear_slot_calls] = slot;
    ++g_clear_slot_calls;
    g_last_slot = slot;
    return 1;
}

int nox_bot_runtime_clear_all_server_created(void)
{
    ++g_clear_all_calls;
    return 2;
}

static int check(int condition, const char *message)
{
    if (condition)
        return 0;
    fprintf(stderr, "bot_console_test: %s\n", message);
    return 1;
}

static void reset_calls(void)
{
    g_attach_calls = 0;
    g_detach_calls = 0;
    g_difficulty_calls = 0;
    g_spawn_calls = 0;
    g_spawn_fail_call = 0;
    g_clear_slot_calls = 0;
    for (g_last_slot = 0; g_last_slot < 6; ++g_last_slot)
        g_cleared_slots[g_last_slot] = -1;
    g_clear_all_calls = 0;
    g_last_object = 0;
    g_last_slot = -1;
    g_last_class = -1;
    g_last_team = NOX_BOT_SPAWN_TEAM_AUTO;
    g_last_difficulty = NOX_BOT_DIFFICULTY_NORMAL;
}

int main(void)
{
    const wchar_t *not_bot[] = { L"map", L"test" };
    const wchar_t *spawn[] = { L"bot", L"spawn", L"red", L"wizard", L"hard" };
    const wchar_t *spawn_3v3[] = { L"bot", L"spawn", L"3v3", L"hardcore" };
    const wchar_t *clear_slot[] = { L"bot", L"clear", L"12" };
    const wchar_t *clear_all[] = { L"bot", L"clear" };
    const wchar_t *attach_default[] = { L"bot", L"attach", L"3" };
    const wchar_t *attach_hard[] = { L"bot", L"attach", L"3", L"hard" };
    const wchar_t *detach[] = { L"bot", L"detach", L"3" };
    const wchar_t *difficulty[] = { L"bot", L"difficulty", L"3", L"beginner" };
    const wchar_t *bad_slot[] = { L"bot", L"attach", L"32" };
    const wchar_t *trace_on[] = { L"bot", L"trace", L"on" };
    const wchar_t *trace_off[] = { L"bot", L"trace", L"off" };
    const wchar_t *trace_status[] = { L"bot", L"trace", L"status" };

    reset_calls();
    if (check(nox_bot_console_command(2, not_bot) == 0, "non-bot command must fall through"))
        return 1;

    g_server = 0;
    if (check(nox_bot_console_command(3, attach_default) == 1, "client bot command must be consumed"))
        return 1;
    if (check(g_attach_calls == 0, "client must not attach a bot"))
        return 1;

    g_server = 1;
    nox_bot_trace_set_enabled(0);
    if (check(nox_bot_console_command(3, trace_on) == 1 && nox_bot_trace_enabled(),
              "trace on did not enable lifecycle diagnostics"))
        return 1;
    if (check(nox_bot_console_command(3, trace_status) == 1, "trace status was not consumed"))
        return 1;
    if (check(nox_bot_console_command(3, trace_off) == 1 && !nox_bot_trace_enabled(),
              "trace off did not disable lifecycle diagnostics"))
        return 1;

    reset_calls();
    if (check(nox_bot_console_command(5, spawn) == 1, "spawn command not consumed"))
        return 1;
    if (check(g_spawn_calls == 1 && g_last_team == NOX_BOT_SPAWN_TEAM_RED &&
              g_last_class == 1 && g_last_difficulty == NOX_BOT_DIFFICULTY_HARD,
              "spawn arguments were not parsed"))
        return 1;

    reset_calls();
    nox_bot_console_command(4, spawn_3v3);
    if (check(g_spawn_calls == 6 && g_last_team == NOX_BOT_SPAWN_TEAM_BLUE &&
              g_last_class == 2 && g_last_difficulty == NOX_BOT_DIFFICULTY_HARDCORE,
              "3v3 did not create the six class/team attempts"))
        return 1;

    reset_calls();
    g_spawn_fail_call = 3;
    nox_bot_console_command(4, spawn_3v3);
    if (check(g_spawn_calls == 3 && g_clear_slot_calls == 2 &&
              g_cleared_slots[0] == 12 && g_cleared_slots[1] == 11,
              "3v3 failure did not roll back previously created bots"))
        return 1;

    reset_calls();
    nox_bot_console_command(3, clear_slot);
    if (check(g_clear_slot_calls == 1 && g_last_slot == 12, "clear slot was not forwarded"))
        return 1;
    reset_calls();
    nox_bot_console_command(2, clear_all);
    if (check(g_clear_all_calls == 1, "clear all was not forwarded"))
        return 1;

    reset_calls();
    if (check(nox_bot_console_command(3, attach_default) == 1, "attach command not consumed"))
        return 1;
    if (check(g_attach_calls == 1 && g_last_object == g_object, "attach did not target occupied slot"))
        return 1;
    if (check(g_last_difficulty == NOX_BOT_DIFFICULTY_NORMAL, "attach default difficulty is not normal"))
        return 1;

    reset_calls();
    nox_bot_console_command(4, attach_hard);
    if (check(g_attach_calls == 1 && g_last_difficulty == NOX_BOT_DIFFICULTY_HARD,
              "attach difficulty was not parsed"))
        return 1;

    reset_calls();
    nox_bot_console_command(3, detach);
    if (check(g_detach_calls == 1 && g_last_object == g_object, "detach did not target occupied slot"))
        return 1;

    reset_calls();
    nox_bot_console_command(4, difficulty);
    if (check(g_difficulty_calls == 1 && g_last_difficulty == NOX_BOT_DIFFICULTY_BEGINNER,
              "difficulty command was not applied"))
        return 1;

    reset_calls();
    nox_bot_console_command(3, bad_slot);
    return check(g_attach_calls == 0, "invalid slot must not attach");
}
