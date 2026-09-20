/* Regression for negative multiplayer scores: Arena suicide/team-kill penalties
 * are signed scores. They must not be interpreted as UINT32_MAX by the normal
 * point-limit victory check. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if UINTPTR_MAX > UINT32_MAX && defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

#ifndef __cdecl
#define __cdecl
#endif

typedef int BOOL;
typedef short __int16;

unsigned char byte_5D4594[3844309];
unsigned char byte_587000[400000];

static __int16 score_limit = 10;
static char *first_team;
static int first_player;
static int team_winner_calls;
static int player_winner_calls;
static int last_winner;

BOOL __cdecl sub_40A5C0(int flag)
{
    (void)flag;
    return 0;
}

int sub_40A5B0(void)
{
    return 0;
}

__int16 __cdecl sub_40A020(__int16 value)
{
    (void)value;
    return score_limit;
}

char *sub_418B10(void)
{
    return first_team;
}

char *__cdecl sub_418B60(int team)
{
    (void)team;
    return 0;
}

int sub_4DA7C0(void)
{
    return first_player;
}

int __cdecl sub_4DA7F0(int player)
{
    (void)player;
    return 0;
}

BOOL __cdecl sub_419130(int object_team)
{
    (void)object_team;
    return 0;
}

char *__cdecl sub_418AB0(int id)
{
    (void)id;
    return 0;
}

int sub_40A8A0(void)
{
    return 1;
}

int __cdecl sub_40A4D0(int flag)
{
    return flag;
}

int __cdecl sub_4D8BF0(int team, char announce)
{
    (void)announce;
    ++team_winner_calls;
    last_winner = team;
    return 1;
}

int __cdecl sub_4D8B90(int player, char announce)
{
    (void)announce;
    ++player_winner_calls;
    last_winner = player;
    return 1;
}

char sub_509A60(void);

static void *test_low_alloc(size_t size)
{
#if UINTPTR_MAX > UINT32_MAX && defined(__linux__)
    size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    size_t mapped_size = (size + page_size - 1) & ~(page_size - 1);
    void *result = mmap(0, mapped_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    return result == MAP_FAILED ? 0 : result;
#else
    return calloc(1, size);
#endif
}

static void test_low_free(void *memory, size_t size)
{
#if UINTPTR_MAX > UINT32_MAX && defined(__linux__)
    size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    size_t mapped_size = (size + page_size - 1) & ~(page_size - 1);
    if (memory)
        munmap(memory, mapped_size);
#else
    (void)size;
    free(memory);
#endif
}

static void reset_winner_state(void)
{
    team_winner_calls = 0;
    player_winner_calls = 0;
    last_winner = 0;
}

int main(void)
{
    unsigned char *team = test_low_alloc(80);
    unsigned char *unit = test_low_alloc(800);
    unsigned char *update = test_low_alloc(320);
    unsigned char *player_info = test_low_alloc(3712);

    if (!team || !unit || !update || !player_info)
        return 5;

    memset(team, 0, sizeof(team));
    memset(unit, 0, sizeof(unit));
    memset(update, 0, sizeof(update));
    memset(player_info, 0, sizeof(player_info));

    /* Team scores are signed. A suicide/team-kill penalty from zero produces
     * -1 and must not satisfy a positive point limit. */
    first_team = (char *)team;
    first_player = 0;
    *(int *)(team + 52) = -1;
    reset_winner_state();
    sub_509A60();
    if (team_winner_calls != 0 || player_winner_calls != 0)
        return 1;

    *(int *)(team + 52) = score_limit;
    sub_509A60();
    if (team_winner_calls != 1 || last_winner != (int)(intptr_t)team)
        return 2;

    /* Player Lessons/score is also signed. Drive the same production victory
     * path with the player-info layout used by sub_509A60. */
    first_team = 0;
    *(uint32_t *)(unit + 748) = (uint32_t)(uintptr_t)update;
    *(uint32_t *)(update + 276) = (uint32_t)(uintptr_t)player_info;
    *(uint32_t *)(player_info + 3680) = 0;
    first_player = (int)(intptr_t)unit;

    *(int *)(player_info + 2136) = -1;
    reset_winner_state();
    sub_509A60();
    if (team_winner_calls != 0 || player_winner_calls != 0)
        return 3;

    *(int *)(player_info + 2136) = score_limit;
    sub_509A60();
    if (player_winner_calls != 1 || last_winner != first_player)
        return 4;

    test_low_free(team, 80);
    test_low_free(unit, 800);
    test_low_free(update, 320);
    test_low_free(player_info, 3712);
    return 0;
}
