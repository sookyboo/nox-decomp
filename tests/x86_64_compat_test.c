/* Regression for 3487996: x86_64 hosts must build the 32-bit game ABI. */
#include <stdint.h>

_Static_assert(sizeof(void *) == 4, "Nox compatibility target must be 32-bit");
_Static_assert(sizeof(uintptr_t) == 4, "Nox pointer slots must remain 32-bit");

int main(void)
{
    return (sizeof(void *) == 4 && sizeof(uintptr_t) == 4) ? 0 : 1;
}
