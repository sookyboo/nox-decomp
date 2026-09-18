/* Regression for 8653a5f: Obliterate effect motion must use the corrected
 * direction-table values and preserve signed offsets during rendering. */
#include <stdint.h>
#include <string.h>

unsigned char byte_5D4594[3844309];
unsigned char byte_587000[400000];

static int moved;
static int removed;
static int moved_x;
static int moved_y;

void __wrap_sub_45A4E0(int object)
{
    removed = object != 0;
}

void __wrap_sub_49AA90(uint32_t *object, int x, int y)
{
    moved = object != 0;
    moved_x = x;
    moved_y = y;
}

int sub_4CA720(int unused, int object);

int main(void)
{
    uint8_t effect[500];
    const unsigned int direction = 21;
    const int table = 192088 + 8 * direction;

    memset(effect, 0, sizeof(effect));
    *(uint32_t *)(byte_5D4594 + 2598000) = 10;
    *(int *)(effect + 12) = 0;
    *(int *)(effect + 16) = 0;
    *(uint16_t *)(effect + 432) = 100;
    *(uint16_t *)(effect + 434) = 200;
    *(int16_t *)(effect + 440) = 160;
    effect[442] = 0;
    effect[443] = 1;
    *(int *)(effect + 316) = 0;

    /* The signed Y component is important: the corrected path must retain
     * the negative direction-table value when calculating the offset. */
    *(int32_t *)(byte_587000 + table) = 16;
    *(int32_t *)(byte_587000 + table + 4) = -16;

    if (sub_4CA720(0, (int)(uintptr_t)effect) != 1)
        return 1;
    if (!moved || moved_x != 234 || moved_y != 66 || removed)
        return 2;
    if (*(int *)(effect + 32) != 234 || *(int *)(effect + 36) != 66)
        return 3;

    /* Once the effect reaches its target neighborhood, the production
     * lifecycle removes it instead of emitting another position. */
    moved = 0;
    removed = 0;
    *(int *)(effect + 12) = 95;
    *(int *)(effect + 16) = 195;
    *(uint32_t *)(byte_5D4594 + 2598000) = 10;
    if (sub_4CA720(0, (int)(uintptr_t)effect) != 0)
        return 4;
    if (!removed || moved)
        return 5;

    return 0;
}
