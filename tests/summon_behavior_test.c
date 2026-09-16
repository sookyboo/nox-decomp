/* Regression for a1b2f49: a summon must survive start through completion. */
#include <stdint.h>
#include <string.h>

unsigned char byte_5D4594[3844309];
unsigned char byte_587000[400000];

static uint8_t summoned[900];
static uint8_t summoned_state[1500];
static uint8_t owner_fixture[800] __attribute__((aligned(64)));
/* The production field carries both a pointer and flag bits. Keep the
 * fixture address clear in those bits so ASLR cannot change the test path. */
static uint8_t template_fixture[100] __attribute__((aligned(65536)));
static uint8_t owner_state[1200] __attribute__((aligned(64)));
static uint8_t owner_data[4200] __attribute__((aligned(64)));
static uint8_t action_fixture[100] __attribute__((aligned(64)));
static int created;
static int created_owner;
static float created_x;
static float created_y;

int sub_40A5C0(int value) { (void)value; return 0; }
int sub_427460(int summon_type) { (void)summon_type; return 0; }
const char *sub_427230(int summon_type) { (void)summon_type; return "test-summon"; }
int sub_4E3AA0(const char *name) { (void)name; return 0x1234; }
int __wrap_sub_535250(void *position, int a, int b, int c)
{ (void)position; (void)a; (void)b; (void)c; return 1; }
int sub_411A90(void *position) { (void)position; return 0; }
int __wrap_sub_5236F0(short id, float *position, char owner, short type, short duration)
{ (void)id; (void)position; (void)owner; (void)type; (void)duration; return 1; }
void sub_4DA2C0(int owner, const char *message, int flags)
{ (void)owner; (void)message; (void)flags; }
void sub_4DAA50(int object, int owner, float x, float y)
{
    created = object == (int)(uintptr_t)summoned;
    created_owner = owner == (int)(uintptr_t)owner_fixture;
    created_x = x;
    created_y = y;
}
uint32_t *sub_4E3450(int type)
{
    (void)type;
    memset(summoned, 0, sizeof(summoned));
    memset(summoned_state, 0, sizeof(summoned_state));
    *(uint32_t *)(summoned + 748) = (uint32_t)(uintptr_t)summoned_state;
    return (uint32_t *)summoned;
}
void __wrap_sub_533900(int owner, int object, int mode)
{
    (void)owner; (void)object; (void)mode;
}
int sub_424800(int value, int mode) { (void)value; (void)mode; return 1; }
void __wrap_sub_501960(int event, int owner, int a, int b)
{ (void)event; (void)owner; (void)a; (void)b; }
int sub_4EC4F0(int unit, int owner) { (void)unit; (void)owner; return 0; }
double sub_419D70(const char *text, int arg) { (void)text; (void)arg; return 0.0; }
int sub_419A70(float value) { (void)value; return 1; }
void sub_4D91A0(int player, int object) { (void)player; (void)object; }
void sub_417190(int player, int object, int mode)
{ (void)player; (void)object; (void)mode; }
void sub_4DF360(int player, int object) { (void)player; (void)object; }
int sub_419130(void *list) { (void)list; return 0; }
void sub_4191D0(int owner, void *list, int mode, int id, int flags)
{ (void)owner; (void)list; (void)mode; (void)id; (void)flags; }
void sub_4DA0F0(int player, int mode, int *value)
{ (void)player; (void)mode; (void)value; }
int sub_417090(int a, int b) { (void)a; (void)b; return 0; }
int sub_4D7EE0(int a, int b) { (void)a; (void)b; return 0; }
int sub_4E5CC0(int a) { (void)a; return 0; }
int sub_419E60(int a, int b) { (void)a; (void)b; return 0; }
int sub_4142F0(int a, int b) { (void)a; (void)b; return 0; }
int sub_4E39D0(int a) { (void)a; return 0; }
int sub_413FE0(int a, int b) { (void)a; (void)b; return 0; }
int sub_414190(int a, int b) { (void)a; (void)b; return 0; }
int sub_4DA7C0(int a, int b) { (void)a; (void)b; return 0; }
int sub_40EBC0(int a, int b) { (void)a; (void)b; return 0; }
int sub_4DA7F0(int a, int b) { (void)a; (void)b; return 0; }
int sub_4E5420(int a, int b) { (void)a; (void)b; return 0; }
int sub_578AC0(int a, int b) { (void)a; (void)b; return 0; }
int sub_4DDE80(int a, int b) { (void)a; (void)b; return 0; }
int sub_424300(int a, int b) { (void)a; (void)b; return 0; }
int nox_vsprintf(char *out, const char *format, void *args)
{ (void)out; (void)format; (void)args; return 0; }

int sub_500DA0(int action);
int sub_5010D0(int action);
int nox_test_sub_500F40(int action, void *out_xy);

#ifdef _WIN32
/* Keep this focused collaborator deterministic when the complete Windows
 * runtime is linked into the test. The Linux builds exercise the ABI wrapper
 * for the same call; Windows' native ABI does not need that wrapper covered
 * here. */
int nox_test_sub_500F40(int action, void *out_xy)
{
    float *out = (float *)out_xy;
    out[0] = *(float *)(action + 52);
    out[1] = *(float *)(action + 56);
    return 1;
}
#endif

int main(void)
{
    uint8_t *owner = owner_fixture;
    uint8_t *template_data = template_fixture;
    uint8_t *action = action_fixture;
    memset(owner, 0, 800);
    memset(template_data, 0, 100);
    memset(action, 0, 100);
    memset(owner_state, 0, sizeof(owner_state));
    memset(owner_data, 0, sizeof(owner_data));
    const float expected_x = 120.0f;
    const float expected_y = 240.0f;

    /* Start as a neutral owner so the fixture reaches placement directly;
     * completion still exercises the owner-linking branch below. */
    *(uint32_t *)(owner + 8) = 0;
#ifdef _WIN32
    /* The decompiled flag test masks bits in this pointer-sized field. Use a
     * flag-safe sentinel on Windows; the focused Windows collaborator does
     * not dereference the template pointer. */
    *(uint32_t *)(owner + 16) = 0x1000;
#else
    *(uint32_t *)(owner + 16) = (uint32_t)(uintptr_t)template_data;
#endif
    *(uint32_t *)(owner + 516) = 0;
    *(uint32_t *)(owner + 748) = (uint32_t)(uintptr_t)owner_state;
    *(uint32_t *)(owner_state + 276) = (uint32_t)(uintptr_t)owner_data;
    *(float *)(action + 52) = expected_x;
    *(float *)(action + 56) = expected_y;
    *(uint32_t *)(action + 4) = 74; /* summon type 0 after the action offset */
    *(uint32_t *)(action + 16) = (uint32_t)(uintptr_t)owner;

    if (sub_500DA0((int)(uintptr_t)action) != 0)
        return 1;
    if (*(uint16_t *)(action + 72) != 0x1234u)
        return 2;
    float stored_x;
    float stored_y;
    memcpy(&stored_x, action + 74, sizeof(stored_x));
    memcpy(&stored_y, action + 78, sizeof(stored_y));
    if (stored_x != expected_x || stored_y != expected_y)
        return 3;

    *(uint32_t *)(owner + 8) = 4;
    *(uint32_t *)(byte_5D4594 + 2598000) = *(uint32_t *)(action + 68) - 1;
    if (sub_5010D0((int)(uintptr_t)action) != 1)
        return 4;
    if (!created || created_x != expected_x || created_y != expected_y)
        return 5;
    if (!created_owner)
        return 6;
    if (!(*(uint32_t *)(summoned + 12) & 0x80u))
        return 7;
    return 0;
}
