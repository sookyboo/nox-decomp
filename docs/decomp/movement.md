- `sub_4E7010(int a1, float2 *a2)`
    - Main “hard set object/unit position” function.
    - Writes `a2` into:
        - `a1 + 56/60` = current position
        - `a1 + 64/68`
        - `a1 + 72/76`
    - Then refreshes movement/collision/state via:
        - `sub_517970`
        - `sub_537610`
        - `sub_537540`
        - `sub_5346D0`
        - `sub_4FD090`
    - For player-controlled objects (`*(_BYTE *)(a1 + 8) & 4`), if not following and not in certain movement states:
        - copies `a1 + 56/60` into owner/player-control data at `+3632/+3636`
    - So `+3632/+3636` are player-control-side movement coordinates, often kept aligned with the controlled unit’s real position.

- `+3632 / +3636`
    - Floats in the player/control structure (`*(_DWORD *)(thing + 748) + 276` path).
    - Represent the current command cursor / smoothed desired movement point.
    - They are:
        - set from a followed target’s `+56/+60`
        - set from the controlled unit’s `+56/+60` by `sub_4E7010`
        - used by movement smoothing and nearby target searches

- `+2284 / +2288`
    - Integer destination coordinates in the same player/control structure.
    - These are the authoritative move-order target coordinates.
    - Actual write found in packet handling:
        - `GAME4.c:32643  *(_DWORD *)(v8 + 2284) = v12;`
        - `GAME4.c:32644  *(_DWORD *)(v8 + 2288) = *((unsigned __int16 *)v4 - 1);`
    - This happens under packet/message opcode `0xAC` (`-84`).

- `0xAC` / `-84` movement message
    - This is the concrete “set move destination” message.
    - Receiver side:
        - `GAME4.c` case `0xAC` writes packet x/y into `+2284/+2288`
    - Sender side:
        - `sub_43C8F0(int a1, int a2, int a3)` builds and sends:
            - first byte `-84`
            - then 16-bit x
            - then 16-bit y
    - So `sub_43C8F0` is the concrete move-target sender, and `0xAC` is the concrete move-target packet.

- `sub_43C8F0(int a1, int a2, int a3)`
    - Sends movement destination updates.
    - Emits packet `-84` with `(a2, a3)` as 16-bit coordinates.
    - Includes duplicate suppression using:
        - `byte_5D4594[815768]`
        - `byte_5D4594[815770]`
    - This appears to be the direct sender for click-to-move destination changes.

- `sub_435F80()`
    - Strong match for the local mouse-driven movement submission path.
    - Reads cursor position through:
        - `sub_4309F0()` → returns pointer to `byte_5D4594[805660]`
    - Computes a world-space destination from cursor/camera state:
        - `x = byte_5D4594[811084] + mouse_x - byte_5D4594[811068]`
        - `y = byte_5D4594[811088] + mouse_y - byte_5D4594[811072]`
    - Then calls:
        - `sub_43C8F0(31, x, y)`
    - This gives the concrete mouse chain:
        - cursor position
        - screen/camera-to-world conversion
        - send `0xAC`
        - receiver stores `+2284/+2288`
        - movement code consumes that target

- `sub_4309F0()`
    - Returns `(int2 *)&byte_5D4594[805660]`
    - This is the current mouse/cursor position storage used by `sub_435F80()`.

- `811084 / 811088`
    - World-space anchor/base values used when converting cursor position to world destination in `sub_435F80()`.

- `811068 / 811072`
    - Viewport/camera position or offset values subtracted during cursor-to-world conversion in `sub_435F80()`.
    - Also passed to viewport/camera update helpers like `sub_49BD70((int)&byte_5D4594[811068])`.

- `sub_4E62F0(int a1)`
    - Key per-player/per-controlled-unit command-processing function.
    - It:
        - clears dead references in a small slot list
        - calls `sub_4E44F0(a1)`
        - may copy followed target position into `+3632/+3636`
        - consumes queued command records via:
            - `sub_51AB50(playerId)`
            - `sub_51ABC0(playerId)`
    - For command type `2`:
        - if `+3672 == 1`, smoothly moves `+3632/+3636` toward `+2284/+2288`
        - uses tolerance/deadzone from `byte_587000[202404]`
        - validates candidate point with `sub_517590(v20, v21)`
        - if valid, updates `+3632/+3636`
    - Therefore:
        - `+2284/+2288` = destination target
        - `+3632/+3636` = smoothed command point moving toward that target
    - For command type `7`:
        - performs nearby search around `+3632/+3636`
        - may call `sub_4E6060(...)` or `sub_4E6040(...)`
        - updates `+3672`
    - Other logic can cancel or replace movement through:
        - `sub_4E60E0`
        - `sub_4E6040`
        - `sub_4DDEF0`

- `+3672`
    - Movement/command mode state in the player/control structure.
    - In `sub_4E62F0`:
        - `+3672 == 1` enables smoothing from `+3632/+3636` toward `+2284/+2288`
        - other values select different behaviour
    - So wrong-feeling movement could be caused by `+3672` being reset or changed.

- `+3628`
    - Followed/attached target pointer.
    - If nonzero, several places copy that target’s `+56/+60` into `+3632/+3636`.
    - So follow/lock-on behaviour can override free destination movement.

- `sub_4DA0F0(int a1, int a2, int *a3)`
    - Small command/event sender.
    - For types `0,1,2,12,13,16,20,21`:
        - sends 6-byte packet:
            - opcode `-87`
            - subcode `a2`
            - payload `*a3`
    - For type `17`:
        - sends 2-byte packet `4521`
    - This looks like a compact command/event wrapper, but not the concrete click-destination packet.

- `sub_51A960(int a1, unsigned __int8 *a2)`
    - Decodes a batch of command entries into a per-player queue.
    - Uses `sub_51AAA0(...)` to unpack entries into 24-byte records.
    - Stores them in per-player queue storage near:
        - `byte_5D4594[2388932 + ...]`
    - Then deduplicates via `sub_51AA20(a1)`.
    - Part of the command ingestion path later consumed by `sub_4E62F0()`.

- `sub_51AB50(int a1)` / `sub_51ABC0(int a1)`
    - Iterators over queued command records for a player.
    - `sub_51AB50` returns first live entry.
    - `sub_51ABC0` returns next live entry.
    - `sub_4E62F0()` uses them each tick to process movement/action commands.

- `sub_4E5390(...)` / `sub_4E5420(...)`
    - Thin wrappers over `sub_4E5030(...)`.
    - Used to send messages/commands with different routing/flags.
    - `sub_4E53C0(...)` shows some traffic goes direct/local through `sub_40EBC0`, otherwise via `sub_4E5420(...)`.

- What this means for movement
    - The normal mouse movement path now looks like:
        - `sub_4309F0()` reads current cursor position
        - `sub_435F80()` converts cursor position to a world destination using `811084/811088` and `811068/811072`
        - `sub_435F80()` calls `sub_43C8F0(31, x, y)`
        - `sub_43C8F0()` sends packet `0xAC` / `-84`
        - receiver writes x/y into `+2284/+2288`
        - `sub_4E62F0()` smoothly drives `+3632/+3636` toward `+2284/+2288`
        - movement then proceeds normally