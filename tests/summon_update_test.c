/* Regression for adfe080: a summoned unit in the guarded state must return
 * before reading its optional summon state, avoiding the crash path. */
#include <stdint.h>
#include <string.h>

static int sight_guard_calls;
unsigned char byte_5D4594[3844309];

int sub_40A5C0(int value) { (void)value; return 0; }
int sub_536FB0(int self, int target, char mode)
{
    (void)self; (void)target; (void)mode; return 0;
}
int sub_5370E0(int self, int target, char mode)
{
    (void)self; (void)target; (void)mode; return 0;
}
int sub_4FF350(int value, char mode)
{
    (void)value; (void)mode; return 0;
}
void sub_528560(int self, int index) { (void)self; (void)index; }
void sub_528610(int self) { (void)self; }
void sub_5286D0(int self, int arg) { (void)self; (void)arg; }
void sub_517F90(void *center, float radius, void *callback, int arg)
{
    (void)center; (void)radius; (void)callback; (void)arg;
}
int sub_415FA0(int min, int max) { (void)min; (void)max; return 0; }
long double sub_5336D0(int self) { (void)self; return 0.0; }

int sub_534A40(int self)
{
    (void)self;
    sight_guard_calls++;
    return 0;
}

void sub_5281F0__abi_raw(void *self);

int main(void)
{
    uint8_t unit[800];

    memset(unit, 0, sizeof(unit));
    *(uint32_t *)(unit + 16) = 0x8000; /* guarded/summoned unit flag */

    sub_5281F0__abi_raw(unit);

    /* The guard is the observable contract for this minimal fixture: the
     * optional state at +748 must not be dereferenced after it rejects the
     * update. */
    return sight_guard_calls == 1 ? 0 : 1;
}
