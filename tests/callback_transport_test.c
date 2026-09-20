/* Native callback registration must retain a host-width function address. */

#include <stdint.h>

#ifndef __cdecl
#define __cdecl
#endif

int sub_43DE20(uintptr_t callback);
int nox_test_invoke_tick_callback(void);

static int callback_calls;

static int __cdecl test_callback(void)
{
    ++callback_calls;
    return 7;
}

int main(void)
{
    if (sub_43DE20((uintptr_t)test_callback) != 1)
        return 1;
    if (nox_test_invoke_tick_callback() != 7 || callback_calls != 1)
        return 2;
    return 0;
}
