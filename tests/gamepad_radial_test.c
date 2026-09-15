/* Regression for d900cd7: gamepad radial-limit transitions must clamp
 * outward motion only when the cursor is inside or latched. */
#include <math.h>

int nox_env_truthy(const char *value)
{
    return value && value[0] == '1';
}

int nox_env_int(const char *name, int default_value)
{
    (void)name;
    return default_value;
}

void nox_test_apply_radial_limit(double *dx, double *dy,
                                 int mouse_x, int mouse_y,
                                 int rmb_down, int enabled, int radius);
void nox_test_apply_radial_limit_latched(double *dx, double *dy,
                                         int mouse_x, int mouse_y, int radius);

static int close_to(double value, double expected)
{
    return fabs(value - expected) < 1e-9;
}

int main(void)
{
    double dx;
    double dy;

    /* Inside the radius: outward motion is removed. */
    dx = 10.0;
    dy = 0.0;
    nox_test_apply_radial_limit(&dx, &dy, 145, 100, 1, 1, 50);
    if (!close_to(dx, 0.0) || !close_to(dy, 0.0))
        return 1;

    /* Exactly at the boundary: the outward step is still clamped. */
    dx = 1.0;
    dy = 0.0;
    nox_test_apply_radial_limit(&dx, &dy, 150, 100, 1, 1, 50);
    if (!close_to(dx, 0.0))
        return 1;

    /* Outside without having entered: preserve motion so the cursor can enter. */
    dx = 10.0;
    dy = 0.0;
    nox_test_apply_radial_limit(&dx, &dy, 160, 100, 1, 1, 50);
    if (!close_to(dx, 10.0))
        return 1;

    /* Entering latches the limiter; the next outward move is clamped. */
    dx = -20.0;
    dy = 0.0;
    nox_test_apply_radial_limit(&dx, &dy, 160, 100, 1, 1, 50);
    dx = 20.0;
    dy = 0.0;
    nox_test_apply_radial_limit_latched(&dx, &dy, 140, 100, 50);
    if (!close_to(dx, 0.0))
        return 1;

    return 0;
}
