# Panic EUD / Nox Reloaded Map Compatibility Testing

This document provides a small, progressive set of public Nox maps for testing the Panic EUD / MemoryHack compatibility layer in `nox-decomp`.

The goal is to start with ordinary Nox behavior, then move through increasingly complex EUD features. When a map fails, the test order should make it easier to identify which compatibility tier is responsible.

## Public map source

The test maps below come from the public **Panic EUD Maps Project**:

- Repository: https://gitlab.com/happysoft3/eud-maps-project
- Public backup map directory: https://gitlab.com/happysoft3/eud-maps-project/-/tree/master/eud_project/backupMaps

> **Important:** A `.map` file is not necessarily the complete runnable map package. EUD maps may also require the matching compiled NoxScript `.nxz` and other files. If a downloaded map does not contain or load its script, use the complete map package from the Panic project/community distribution rather than treating a standalone `.map` as a compatibility failure.

## Recommended test order

### 0. `estate` — vanilla regression baseline

Before testing any EUD map, verify that an ordinary stock Nox multiplayer map such as `estate` still loads, hosts, and plays normally.

This checks that the compatibility layer has not regressed normal map loading or gameplay.

Expected result:

- map loads normally;
- hosting/joining still works;
- normal NoxScript behavior is unchanged;
- no EUD compatibility code should be required.

---

### 1. `G_Quest.map` — first EUD smoke test

**Start here.**

Direct download:

https://gitlab.com/happysoft3/eud-maps-project/-/raw/master/eud_project/backupMaps/G_Quest.map

Source:

https://gitlab.com/happysoft3/eud-maps-project/-/blob/master/eud_project/g_quest.c

Primary coverage:

- `UnitToPtr`;
- `GetMemory` / `SetMemory`;
- basic object-extension access;
- Tier 1 and Tier 2 pointer/memory compatibility;
- basic Tier 3 semantic helper behavior.

Suggested checks:

- map loads without a crash;
- scripted objects appear correctly;
- movers/objects manipulated by the script behave normally;
- map can run for several minutes without memory corruption.

If this map fails, fix it before moving to the more complex maps.

---

### 2. `G_Graves.map` — broader Tier 2/3 test

Direct download:

https://gitlab.com/happysoft3/eud-maps-project/-/raw/master/eud_project/backupMaps/G_Graves.map

Source:

https://gitlab.com/happysoft3/eud-maps-project/-/blob/master/eud_project/g_graves.c

Primary coverage:

- `UnitToPtr`;
- `GetMemory` / `SetMemory`;
- wall helpers;
- object creation;
- simple FX helpers;
- additional Tier 3 native-operation translation.

Suggested checks:

- map initializes correctly;
- walls are found/created/modified correctly;
- scripted grave/object interactions work;
- effects appear without crashes;
- no raw EUD code is executed.

---

### 3. `!test.map` — heavier object/memory compatibility

Direct download:

https://gitlab.com/happysoft3/eud-maps-project/-/raw/master/eud_project/backupMaps/%21test.map

Source:

https://gitlab.com/happysoft3/eud-maps-project/-/blob/master/eud_project/%21test.c

Primary coverage:

- extensive object structure access;
- object creation;
- monster data manipulation;
- pointer-token traversal;
- additional native-handler/function-field cases.

This is a useful stress test after `G_Quest` and `G_Graves` work.

> Some behavior in this map may still exercise native handler values that have not yet received a semantic compatibility implementation. A failure here does not necessarily indicate that the basic EUD memory layer is broken.

Suggested checks:

- map loads and initializes;
- custom monsters/objects appear;
- basic combat and scripted mechanics work;
- record the first unsupported handler/address if behavior stops.

---

### 4. `Monster.map` — callback compatibility test

Direct download:

https://gitlab.com/happysoft3/eud-maps-project/-/raw/master/eud_project/backupMaps/Monster.map

Source:

https://gitlab.com/happysoft3/eud-maps-project/-/blob/master/eud_project/monster.c

Primary coverage:

- Tier 4 callback replacement;
- `SetUnitCallbackOnCollide`;
- `SetUnitCallbackOnUseItem`;
- spell helpers;
- memory allocation helpers;
- custom unit behavior;
- Tier 4.5 / Tier 5 interaction.

Suggested checks:

- callback-installed objects initialize correctly;
- collision callbacks fire at the expected time;
- item-use callbacks receive the expected caller/trigger context;
- callback return values do not break normal handler dispatch;
- object deletion/reuse does not inherit stale callbacks.

> `Monster.map` also uses some native update-handler values outside the compatibility set implemented so far. It is therefore both a test map and a useful source of future compatibility gaps.

---

### 5. `Dim.map` — advanced gap-discovery map

Direct download:

https://gitlab.com/happysoft3/eud-maps-project/-/raw/master/eud_project/backupMaps/Dim.map

Source:

https://gitlab.com/happysoft3/eud-maps-project/-/blob/master/eud_project/dim.c

Primary coverage includes advanced Panic libraries such as:

- binding/call helpers;
- melee attack replacement;
- advanced pickup replacement;
- potion extensions;
- larger copied-code helpers.

This map should **not** currently be treated as a required pass condition. It is useful after the simpler maps pass, because failures identify candidates for later semantic ports.

---

### 6. `!atest.map` — raw-code stress / unsupported-feature discovery

Direct download:

https://gitlab.com/happysoft3/eud-maps-project/-/raw/master/eud_project/backupMaps/%21atest.map

Source:

https://gitlab.com/happysoft3/eud-maps-project/-/blob/master/eud_project/%21atest.c

This map deliberately uses advanced MemoryHack techniques including generated machine code and raw-code invocation.

Do **not** use this as an initial pass/fail test.

It is useful only after the supported maps work, to discover features that still require semantic C implementations.

## Expected compatibility progression

| Test | Main purpose | Expected status |
| --- | --- | --- |
| `estate` | Vanilla regression | Must pass |
| `G_Quest.map` | Basic EUD memory/pointers | Should be first EUD pass target |
| `G_Graves.map` | Memory + walls + FX | Should largely work |
| `!test.map` | Heavy object manipulation | Partial-to-good; useful for gap discovery |
| `Monster.map` | Callback / advanced semantic compatibility | Major Tier 4/5 test |
| `Dim.map` | Large copied-code features | Expected remaining gaps |
| `!atest.map` | Raw-code stress | Expected remaining gaps |

## Installing a downloaded map

Original Nox maps are normally stored using a matching directory and map basename, for example:

```text
maps/
  G_Quest/
    G_Quest.map
    G_Quest.nxz      # when supplied/required
    ...other files
```

Keep all files belonging to the map together.

Do not rename only the `.map` file without also matching the directory/script names expected by the map package.

## Temporary first-map smoke test

The repository has an opt-in package smoke test for the first EUD target,
`G_Quest`. It is disabled by default and requires the cloned maps project plus
the EUD compatibility feature:

```sh
cmake -S . -B build-i386 \
  -DUSE_EUD_COMPAT=ON \
  -DUSE_EUD_MAP_TESTS=ON \
  -DEUD_MAP_PROJECT_DIR="$PWD/build-deps/eud-maps-project" \
  -DEUD_GAMEFILES_DIR="$PWD/build-deps/gamefiles/app"
cmake --build build-i386 --target eud_gquest_map_test
ctest --test-dir build-i386 -R '^eud_gquest_map_test$' --output-on-failure
```

This temporary test verifies that the cloned `G_Quest.map` and `g_quest.c`
are present, contain the expected first-tier EUD operations, and are paired
with the Nox gamefiles. It does not claim gameplay compatibility: the current
checkout lacks the complete compiled `G_Quest.nxz` package, and automated
launch is also display/OpenGL dependent.

The next-map package smoke test checks `G_Graves.map`, `g_graves.c`, the Nox
gamefiles, and the three custom GRP resources listed by the map's resource
manifest:

```sh
cmake --build build-i386 --target eud_ggraves_map_test
ctest --test-dir build-i386 -R '^eud_ggraves_map_test$' --output-on-failure
```

As with `G_Quest`, this confirms package inputs only; it does not execute the
map script or validate gameplay behavior.

The following smoke test covers the heavier `!test.map` target:

```sh
cmake --build build-i386 --target eud_bangtest_map_test
ctest --test-dir build-i386 -R '^eud_bangtest_map_test$' --output-on-failure
```

It checks the map, `!test.c`, and the Nox gamefiles. The map has no separate
custom resource manifest in the cloned project. Its compiled `.nxz` and actual
runtime behavior remain unverified, especially for native handler values that
are intentionally outside the current compatibility boundary.

## Linux headless OpenGL

The server container runs the 32-bit game through an X11 display provided by
Xvfb and uses Mesa software rendering. On Ubuntu/Debian, install the runtime
packages used by `dist-scripts/Dockerfile.x86`:

```sh
sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install \
  xvfb libgl1-mesa-dri libglx-mesa0 libegl1 libegl-mesa0 mesa-utils \
  libc6:i386 libstdc++6:i386 zlib1g:i386 libgcc-s1:i386 \
  libasound2:i386 libpulse0:i386 libsdl2-2.0-0:i386 \
  libx11-6:i386 libxext6:i386 libxrandr2:i386 libxi6:i386 \
  libxcursor1:i386 libxinerama1:i386 libgl1:i386 libopenal1:i386
```

Some distributions provide the Mesa EGL loader under a different package
name. For example, current Ubuntu releases use `libegl1` and `libegl-mesa0`
instead of the older `libegl1-mesa` name used by the container recipe.

Run from the gamefiles directory, with the map installed under `maps/`, using
the same environment as `dist-scripts/server.sh`:

```sh
cd build-deps/gamefiles/app
export ALSOFT_DRIVERS=null
export LIBGL_ALWAYS_SOFTWARE=1
export SDL_VIDEODRIVER=x11
xvfb-run -a -s "-screen 0 1280x720x24" \
  /absolute/path/to/build-i386/src/out -serveronly G_Quest
```

Do not use `SDL_VIDEODRIVER=dummy` for this test: SDL's dummy driver does not
provide an OpenGL context. `LIBGL_ALWAYS_SOFTWARE=1` makes the result
independent of a host GPU; `mesa-utils` can verify the setup with:

```sh
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a \
  -s "-screen 0 1280x720x24" glxinfo -B
```

The expected renderer is Mesa `llvmpipe`. This only validates the headless
display/OpenGL environment; actual `G_Quest` gameplay still requires the
complete map package, including its compiled `.nxz` when supplied.

## Building an NXZ script package

`tools/build_eud_nxz.py` is an opt-in utility for producing a map script
package. It accepts an existing EUD compiler `SCRIPT03` object, or invokes the
Windows `eudcc.exe` shipped in the Panic project through Wine and then writes
the resulting `.nxz`:

```sh
# Package an already compiled SCRIPT03 object.
python3 tools/build_eud_nxz.py \
  --object build-deps/gamefiles/app/nc.obj \
  --output build-deps/gamefiles/app/maps/G_Quest/G_Quest.nxz

# Compile a source file with the bundled Windows compiler, then package it.
xvfb-run -a python3 tools/build_eud_nxz.py \
  --source build-deps/eud-maps-project/eud_project/g_quest.c \
  --include-dir build-deps/eud-maps-project/eud_project \
  --output build-deps/gamefiles/app/maps/G_Quest/G_Quest.nxz
```

The source form requires Wine and a complete EUD compiler project. The current
clone's `g_quest.c` includes `libs/define.h`, which references the project's
`noxscript/builtins.h`; if that header is not present, compilation must be
completed with the matching Panic/NoxScript headers before this utility can
produce a gameplay script. Compiler diagnostics are not hidden, and incomplete
compiler output is rejected.

The generated stream uses the original NXZ format's initial Huffman table and
literal symbols. It is intentionally conservative and may be larger than a
map editor's adaptive compression. Resource injection (custom images, sounds,
or other map resources) remains a separate step; this utility only packages
the compiled script object.

## What to record when a map fails

For each test, record:

```text
Map:
Build/commit:
Host or client:
Single-player or multiplayer:
Loaded successfully: yes/no
Crash: yes/no
First visible incorrect behavior:
Last mechanic that worked:
Console/error output:
Unsupported EUD address/helper, if reported:
Reproduction steps:
```

For crashes, also retain the native backtrace/core dump when available.

The most useful report is the **first reproducible divergence**, rather than a list of later failures caused by the same missing feature.

## Recommended workflow

Test in this order:

```text
estate
  ↓
G_Quest
  ↓
G_Graves
  ↓
!test
  ↓
Monster
  ↓
Dim / !atest for gap discovery
```

Do not move to the next map if an earlier basic map exposes a reproducible compatibility failure that has not been understood yet.

## Safety/compatibility goal

The EUD compatibility layer should preserve this rule throughout testing:

```text
known Panic EUD operation
        ↓
portable semantic C implementation
```

It should never fall back to:

```text
arbitrary EUD address
        ↓
execute injected x86 / dereference arbitrary host memory
```

A map failing safely because an unsupported helper is rejected is preferable to executing unknown MemoryHack code.
