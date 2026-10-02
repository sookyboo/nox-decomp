# Docker sandbox development environment

The Frida-enabled sandbox environment is described by
`sbxenv-frida.yaml`; it references the kit in
`tools/docker-sandbox/nox-decomp-dev/spec.yaml`, which installs
cross-compilers, game build dependencies, Xvfb, and Frida's command line
tools. The target defaults to `i386`; select it when creating the sandbox for
the Linux 32-bit game target.

In this workspace, `sbxenv.yaml` may be mounted read-only. When a requested
environment-manifest change cannot be written there, create the revised
manifest under a new filename (for example, `sbxenv-frida.yaml`) and leave it
uncommitted for the maintainer to commit separately. Keep the kit implementation
and documentation changes in their normal tracked files and commit those
independently.

The kit installs `frida-tools` into `/opt/frida-tools`, a dedicated Python
virtual environment, and exposes `frida`, `frida-ps`, and `frida-trace` in
`/usr/local/bin`. For a manually prepared Ubuntu/Debian sandbox, install Frida
the same way without changing the distribution's system Python:

```sh
sudo apt-get update
sudo apt-get install -y python3-venv
sudo python3 -m venv /opt/frida-tools
sudo /opt/frida-tools/bin/pip install --upgrade pip frida-tools
for tool in frida frida-ps frida-trace; do
  sudo ln -sf "/opt/frida-tools/bin/$tool" "/usr/local/bin/$tool"
done
```

## Instrumenting the Linux game with Frida

Build the native i386 executable and keep the target game files in a disposable
directory. Start Frida from that game-files directory so the game inherits the
expected working directory. For example, launch the game under Frida with a
JavaScript probe:

```sh
cd build-deps/gamefiles/app
frida -f ../../../build-i386/src/out \
  -l /absolute/path/to/probe.js
```

Or attach to an already running process:

```sh
frida-ps -a
frida -p GAME_PID -l /absolute/path/to/probe.js
```

Frida scripts used during Nox investigations locate the main executable with
`Process.enumerateModules()`, attach with `Interceptor.attach()`, and log
observable boundaries such as map-file opens, input injection, console command
dispatch, and process exit. For rebuilt binaries, get function addresses from
`nm -n build-i386/src/out` and use the module base plus the ELF symbol's
relative address in the probe. Keep probes and logs in `/tmp` unless they are
intended to become maintained diagnostics. Frida spawn/attach requires the
sandbox kernel to permit tracing its own child processes.

## Running the game in server mode

The i386 game still needs an X11/OpenGL context when running headlessly; use
Xvfb and Mesa software rendering from the game-files directory:

```sh
cd build-deps/gamefiles/app
ALSOFT_DRIVERS=null LIBGL_ALWAYS_SOFTWARE=1 SDL_VIDEODRIVER=x11 \
NOX_GAMEPAD=0 NOX_NO_INTERNET_SERVERS=1 NOX_UPNP_ENABLE=0 \
xvfb-run -a -s '-screen 0 1280x720x24' \
  ../../../build-i386/src/out -serveronly capflag
```

`-serveronly <map>` starts the game's dedicated server path directly. For an
automated multiplayer host through the front end, enable the control server and
run the `multiplayerHostMenus` boot macro instead:

```sh
cd build-deps/gamefiles/app
ALSOFT_DRIVERS=null LIBGL_ALWAYS_SOFTWARE=1 SDL_VIDEODRIVER=x11 \
NOX_GAMEPAD=0 NOX_NO_INTERNET_SERVERS=1 NOX_UPNP_ENABLE=0 \
NOX_CONTROL_SERVER=1 NOX_CONTROL_SERVER_PASSWORD=secret \
NOX_CONTROL_SERVER_BIND=127.0.0.1 NOX_CONTROL_SERVER_PORT=2323 \
NOX_CONTROL_SERVER_SLEEP_SCALE=1 NOX_CONTROL_LOG=1 \
NOX_CHARACTER_NAME=NoxWarrior NOX_SERVER_NAME=NoxDecompServ \
NOX_SERVER_DEFAULT_MAP=capflag \
'NOX_CONTROL_SERVER_BOOT=sleep 5000; macro multiplayerHostMenus;' \
xvfb-run -a -s '-screen 0 1280x720x24' \
  ../../../build-i386/src/out
```

The host macro enters the multiplayer menus, uses F1 to open the in-game
console, and loads the map named by `NOX_SERVER_DEFAULT_MAP`; this is distinct
from `-serveronly`. It accepts a character and writes a profile under `Save/`,
so use disposable game data. For the UI-only path that stops before the server
console and map load, use `multiplayerHostMenusBeforeGo`. The macro steps,
control-server diagnostics, and runtime verification notes are in
[`docs/multiplayer.md`](../../docs/multiplayer.md#front-end-menu-automation).
