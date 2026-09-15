/* Regression for 1cc83e6: laser geometry must use the assembly's 1-based
 * direction slots and edge ordering. */
#include <stdint.h>
#include <string.h>

unsigned char byte_5D4594[3844309];
unsigned char byte_587000[400000];

uint64_t sub_499290(int slot);
int sub_4992B0(int x, int y);

int main(void)
{
    uint64_t vector;

    /* Direction slots are 1-based in the original data table. */
    *(int32_t *)(byte_5D4594 + 1212068) = 3;
    *(int32_t *)(byte_5D4594 + 1212072) = -4;
    vector = sub_499290(0);
    if ((int32_t)(uint32_t)vector != 3 ||
        (int32_t)(uint32_t)(vector >> 32) != -4)
        return 1;

    *(int32_t *)(byte_5D4594 + 1212076) = -7;
    *(int32_t *)(byte_5D4594 + 1212080) = 9;
    vector = sub_499290(1);
    if ((int32_t)(uint32_t)vector != -7 ||
        (int32_t)(uint32_t)(vector >> 32) != 9)
        return 2;

    /* A square target polygon exercises the wrapped last->first edge and
     * both horizontal and diagonal ray crossings. */
    *(int32_t *)(byte_5D4594 + 1217464) = 4;
    *(int32_t *)(byte_5D4594 + 1203876) = 0;
    *(int32_t *)(byte_5D4594 + 1203880) = 0;
    *(int32_t *)(byte_5D4594 + 1203884) = 100;
    *(int32_t *)(byte_5D4594 + 1203888) = 0;
    *(int32_t *)(byte_5D4594 + 1203892) = 100;
    *(int32_t *)(byte_5D4594 + 1203896) = 100;
    *(int32_t *)(byte_5D4594 + 1203900) = 0;
    *(int32_t *)(byte_5D4594 + 1203904) = 100;

    if (!sub_4992B0(50, 50))
        return 3;
    if (sub_4992B0(150, 50))
        return 4;
    if (sub_4992B0(50, 150))
        return 5;

    return 0;
}
