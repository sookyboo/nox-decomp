# Gamepad radial-limit behavior

The gamepad mouse path enters through `nox_ctrl_inject_mouse_move` in
`src/input.c`, which scales the relative motion and calls
`nox_apply_radial_limit` with the gamepad source. The limiter uses the current
viewport and game-mouse position, a configurable radius, and the injected
right-mouse-button state.

While the cursor is inside the circle, outward radial motion is removed while
tangential motion is preserved. Motion that starts outside the circle is
allowed to enter it. Once an outside-to-inside transition occurs while the
button is held, the limiter latches and prevents subsequent outward escape;
button release clears the latch. Invalid viewports, disabled limits, and
non-positive radii bypass limiting.

This lifecycle is inferred from `nox_apply_radial_limit` and its callers. The
`d900cd7` regression in `tests/gamepad_radial_test.c` covers inside, exact
boundary, outside, and latch transitions with deterministic game-space
fixtures. SDL device polling and rendered cursor behavior remain integration
coverage.

## Mouse scaling

`do_mouse_movement()` in `src/gamepad.c` is the gamepad-to-mouse production
path. It combines D-pad and left-stick input, applies the configured base
percentage, checks whether the configured slow-mouse binding is held, and
injects the resulting relative delta through `nox_ctrl_inject_mouse_move()`.
The slow-mouse setting is a percentage: a value of `40` retains 40% of each
axis delta. The `43df06e` fix corrected this from dividing by the percentage to
multiplying by it. `tests/gamepad_mouse_scaling_test.c` drives the production
function with deterministic stick input and verifies several configured
percentages on both axes. SDL polling and actual cursor movement remain
integration coverage.
