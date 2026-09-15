/* Regression for 5e69080: the Force-of-Nature effect must use the corrected
 * shared distance lookup when advancing toward its target. */
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

unsigned int sub_48C6B0(int x, int y);
int sub_4CA650(int unused, int object);

static void set_distance_fixture(int dx, int dy, unsigned char lookup_value)
{
    unsigned int sum = (unsigned int)(dx * dx + dy * dy);
    unsigned int index = 155956u + (sum >> 6);

    byte_587000[index] = lookup_value;
}

int main(void)
{
    uint8_t effect[500];
    const int target_x = 100;

    memset(effect, 0, sizeof(effect));
    set_distance_fixture(target_x, 0, 200);

    /* sub_4CA650 reads current position at +12/+16, target words at
     * +432/+434, and the effect speed at +443. */
    *(int *)(effect + 12) = 0;
    *(int *)(effect + 16) = 0;
    *(uint16_t *)(effect + 432) = target_x;
    *(uint16_t *)(effect + 434) = 0;
    effect[443] = 10;

    if (sub_48C6B0(target_x, 0) != 100)
        return 1;
    if (sub_4CA650(0, (int)(uintptr_t)effect) != 1)
        return 2;
    if (!moved || moved_x != 9 || moved_y != 0 || removed)
        return 3;

    /* A short remaining distance is consumed instead of advanced. */
    moved = 0;
    removed = 0;
    *(int *)(effect + 12) = 95;
    if (sub_4CA650(0, (int)(uintptr_t)effect) != 0)
        return 4;
    if (!removed || moved)
        return 5;

    return 0;
}
