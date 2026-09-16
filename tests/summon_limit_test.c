/* Regression for 736e9f2: the summon limit must reject a fifth creature. */
#include <stdbool.h>
#include <stdint.h>

static int pending_summon_count;
static int last_requested_type;
unsigned char byte_5D4594[3844309];
unsigned char byte_587000[400000];

int sub_4EC4F0(int unit, int owner)
{
    (void)unit; (void)owner; return 0;
}

int sub_4E3AA0(const char *name)
{
    (void)name; return 0;
}

int sub_427460(int summon_type)
{
    last_requested_type = summon_type;
    return pending_summon_count;
}

bool sub_500D70(intptr_t owner, int summon_type);

static int allowed(unsigned char *owner, int pending, int summon_type)
{
    pending_summon_count = pending;
    last_requested_type = -1;
    if (!sub_500D70((intptr_t)owner, summon_type))
        return 0;
    return last_requested_type == summon_type;
}

int main(void)
{
    unsigned char owner[520] = {0};

    /* Four total creatures is the inclusive limit. */
    if (!allowed(owner, 4, 74))
        return 1;
    if (allowed(owner, 5, 74))
        return 2;

    /* Pending creatures count toward the same limit. */
    if (!allowed(owner, 3, 75))
        return 3;
    if (allowed(owner, 5, 75))
        return 4;

    return 0;
}
