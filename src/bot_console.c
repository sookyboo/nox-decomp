#include "bot_console.h"

#include "bot_engine.h"
#include "bot_policy.h"
#include "bot_runtime.h"
#include "bot_trace.h"
#include "proto.h"

#include <wchar.h>

static void nox_bot_console_print(wchar_t *text)
{
    sub_450C00(6u, text);
}

static void nox_bot_console_print_slot(wchar_t *text, int slot)
{
    sub_450C00(6u, text, slot);
}

static int nox_bot_console_parse_slot(const wchar_t *text, int *slot)
{
    int value = 0;
    const wchar_t *p;

    if (!text || !*text || !slot)
        return 0;
    for (p = text; *p; ++p) {
        if (*p < L'0' || *p > L'9')
            return 0;
        value = value * 10 + (*p - L'0');
        if (value >= NOX_BOT_PLAYER_SLOTS)
            return 0;
    }
    *slot = value;
    return 1;
}

static int nox_bot_console_parse_difficulty(
    const wchar_t *text, nox_bot_difficulty *difficulty)
{
    if (!text || !difficulty)
        return 0;
    if (!_nox_wcsicmp(text, L"hardcore"))
        *difficulty = NOX_BOT_DIFFICULTY_HARDCORE;
    else if (!_nox_wcsicmp(text, L"hard"))
        *difficulty = NOX_BOT_DIFFICULTY_HARD;
    else if (!_nox_wcsicmp(text, L"normal"))
        *difficulty = NOX_BOT_DIFFICULTY_NORMAL;
    else if (!_nox_wcsicmp(text, L"easy"))
        *difficulty = NOX_BOT_DIFFICULTY_EASY;
    else if (!_nox_wcsicmp(text, L"beginner"))
        *difficulty = NOX_BOT_DIFFICULTY_BEGINNER;
    else
        return 0;
    return 1;
}

static int nox_bot_console_parse_team(const wchar_t *text, nox_bot_spawn_team *team)
{
    if (!text || !team)
        return 0;
    if (!_nox_wcsicmp(text, L"red"))
        *team = NOX_BOT_SPAWN_TEAM_RED;
    else if (!_nox_wcsicmp(text, L"blue"))
        *team = NOX_BOT_SPAWN_TEAM_BLUE;
    else if (!_nox_wcsicmp(text, L"auto"))
        *team = NOX_BOT_SPAWN_TEAM_AUTO;
    else
        return 0;
    return 1;
}

static int nox_bot_console_parse_class(const wchar_t *text, int *player_class)
{
    if (!text || !player_class)
        return 0;
    if (!_nox_wcsicmp(text, L"warrior"))
        *player_class = 0;
    else if (!_nox_wcsicmp(text, L"wizard"))
        *player_class = 1;
    else if (!_nox_wcsicmp(text, L"conjurer"))
        *player_class = 2;
    else
        return 0;
    return 1;
}

static void nox_bot_console_usage(void)
{
    nox_bot_console_print(L"bot spawn <red|blue|auto> <warrior|wizard|conjurer> [difficulty]");
    nox_bot_console_print(L"bot spawn 3v3 [hardcore|hard|normal|easy|beginner]");
    nox_bot_console_print(L"bot clear [all|slot]");
    nox_bot_console_print(L"bot attach <slot> [hardcore|hard|normal|easy|beginner]");
    nox_bot_console_print(L"bot detach <slot>");
    nox_bot_console_print(L"bot difficulty <slot> <hardcore|hard|normal|easy|beginner>");
    nox_bot_console_print(L"bot trace <on|off|status>");
}

static int nox_bot_console_spawn_3v3(nox_bot_difficulty difficulty)
{
    static const nox_bot_spawn_team teams[6] = {
        NOX_BOT_SPAWN_TEAM_RED, NOX_BOT_SPAWN_TEAM_RED, NOX_BOT_SPAWN_TEAM_RED,
        NOX_BOT_SPAWN_TEAM_BLUE, NOX_BOT_SPAWN_TEAM_BLUE, NOX_BOT_SPAWN_TEAM_BLUE,
    };
    static const int classes[6] = { 0, 1, 2, 0, 1, 2 };
    int slots[6];
    int count = 0;
    int i;

    for (i = 0; i < 6; ++i) {
        if (!nox_bot_runtime_spawn_attempt(teams[i], classes[i], difficulty, &slots[count])) {
            while (count > 0)
                nox_bot_runtime_clear_server_created(slots[--count]);
            return 0;
        }
        ++count;
    }
    return 1;
}

int nox_bot_console_command(int argc, const wchar_t *const *argv)
{
    nox_bot_difficulty difficulty;
    nox_bot_spawn_team team;
    int player_class;
    int object;
    int slot;

    if (argc <= 0 || !argv || !argv[0] || _nox_wcsicmp(argv[0], L"bot"))
        return 0;

    /* Bot lifecycle and policy are authoritative server state. */
    if (!sub_40A5C0(1)) {
        nox_bot_console_print(L"bot commands are server-side only");
        return 1;
    }

    if (argc == 1 || !_nox_wcsicmp(argv[1], L"help")) {
        nox_bot_console_usage();
        return 1;
    }

    if (!_nox_wcsicmp(argv[1], L"trace")) {
        if (argc != 3) {
            nox_bot_console_usage();
            return 1;
        }
        if (!_nox_wcsicmp(argv[2], L"on")) {
            nox_bot_trace_set_enabled(1);
            nox_bot_console_print(L"bot trace: enabled");
            return 1;
        }
        if (!_nox_wcsicmp(argv[2], L"off")) {
            nox_bot_trace_set_enabled(0);
            nox_bot_console_print(L"bot trace: disabled");
            return 1;
        }
        if (!_nox_wcsicmp(argv[2], L"status")) {
            nox_bot_console_print(nox_bot_trace_enabled() ?
                L"bot trace: enabled" : L"bot trace: disabled");
            return 1;
        }
        nox_bot_console_usage();
        return 1;
    }

    if (!_nox_wcsicmp(argv[1], L"spawn")) {
        difficulty = NOX_BOT_DIFFICULTY_NORMAL;
        if (argc >= 3 && !_nox_wcsicmp(argv[2], L"3v3")) {
            if ((argc != 3 && argc != 4) ||
                (argc == 4 && !nox_bot_console_parse_difficulty(argv[3], &difficulty))) {
                nox_bot_console_usage();
                return 1;
            }
            if (!nox_bot_console_spawn_3v3(difficulty))
                nox_bot_console_print(L"bot spawn 3v3: failed; created bots were rolled back where possible");
            else
                nox_bot_console_print(L"bot spawn 3v3: spawn attempt completed");
            return 1;
        }
        if ((argc != 4 && argc != 5) || !nox_bot_console_parse_team(argv[2], &team) ||
            !nox_bot_console_parse_class(argv[3], &player_class) ||
            (argc == 5 && !nox_bot_console_parse_difficulty(argv[4], &difficulty))) {
            nox_bot_console_usage();
            return 1;
        }
        if (!nox_bot_runtime_spawn_attempt(team, player_class, difficulty, &slot))
            nox_bot_console_print(L"bot spawn: failed; enable bot trace and inspect lifecycle phases");
        else
            nox_bot_console_print_slot(L"bot spawn: server-created bot attempt completed in slot %d", slot);
        return 1;
    }

    if (!_nox_wcsicmp(argv[1], L"clear")) {
        if (argc == 2 || (argc == 3 && !_nox_wcsicmp(argv[2], L"all"))) {
            nox_bot_runtime_clear_all_server_created();
            nox_bot_console_print(L"bot clear: cleared server-created bot slots where removal completed");
            return 1;
        }
        if (argc != 3 || !nox_bot_console_parse_slot(argv[2], &slot)) {
            nox_bot_console_usage();
            return 1;
        }
        if (!nox_bot_runtime_clear_server_created(slot))
            nox_bot_console_print(L"bot clear: slot is not an owned server-created bot or removal failed");
        else
            nox_bot_console_print(L"bot clear: server-created bot removed");
        return 1;
    }

    if (!_nox_wcsicmp(argv[1], L"attach")) {
        difficulty = NOX_BOT_DIFFICULTY_NORMAL;
        if ((argc != 3 && argc != 4) || !nox_bot_console_parse_slot(argv[2], &slot) ||
            (argc == 4 && !nox_bot_console_parse_difficulty(argv[3], &difficulty))) {
            nox_bot_console_usage();
            return 1;
        }
        object = nox_bot_engine_player_object_by_slot(slot);
        if (!object) {
            nox_bot_console_print(L"bot attach: player slot is not occupied");
            return 1;
        }
        if (!nox_bot_runtime_attach_existing_player(object, difficulty)) {
            nox_bot_console_print(L"bot attach: player could not enter native bot control");
            return 1;
        }
        nox_bot_console_print(L"bot attach: native bot control enabled");
        return 1;
    }

    if (!_nox_wcsicmp(argv[1], L"detach")) {
        if (argc != 3 || !nox_bot_console_parse_slot(argv[2], &slot)) {
            nox_bot_console_usage();
            return 1;
        }
        object = nox_bot_engine_player_object_by_slot(slot);
        if (!object) {
            nox_bot_console_print(L"bot detach: player slot is not occupied");
            return 1;
        }
        if (!nox_bot_runtime_detach_existing_player(object)) {
            nox_bot_console_print(L"bot detach: slot is not an attached native bot");
            return 1;
        }
        nox_bot_console_print(L"bot detach: normal player control restored");
        return 1;
    }

    if (!_nox_wcsicmp(argv[1], L"difficulty")) {
        if (argc != 4 || !nox_bot_console_parse_slot(argv[2], &slot) ||
            !nox_bot_console_parse_difficulty(argv[3], &difficulty)) {
            nox_bot_console_usage();
            return 1;
        }
        object = nox_bot_engine_player_object_by_slot(slot);
        if (!object || !nox_bot_runtime_set_difficulty(object, difficulty)) {
            nox_bot_console_print(L"bot difficulty: slot is not an attached native bot");
            return 1;
        }
        nox_bot_console_print(L"bot difficulty: updated");
        return 1;
    }

    nox_bot_console_usage();
    return 1;
}
