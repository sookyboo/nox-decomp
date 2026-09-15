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
