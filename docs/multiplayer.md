The network flow for nox-decomp is something like this

## Front-end menu automation

The control server has two main-thread UI wait commands:

- `waitclick "Caption" timeout_ms` searches live widget trees for the exact
  ASCII caption, waits until the center stays unchanged for five seconds, then
  queues a physical mouse click;
- `waitwidget <root> <id> timeout_ms` performs the same wait for a known widget
  ID under `legal`, `mainmenu`, `window`, `menu`, `servermenu`, `serverscreen`,
  `noxworld`, `charselect`, `classselect`, `charcreate`, `serveroptions`, or
  `any`.

These commands do not call widget handlers directly. The resolved click is
inserted ahead of later commands in its macro batch. If a wait times out, the
remaining commands in that batch are canceled. While waiting, the control
server also searches for the exact caption `Please wait`, logs when that modal
appears/disappears (including the observed duration, coordinates, and root and
widget IDs), and defers the target click until the modal is gone. Set
`NOX_CONTROL_LOG=1` to enable these diagnostics.

The main-menu captions and fallback widget IDs are derived from the loaded
`.wnd` trees: `Multiplay` is widget 112 in `MainMenu.wnd`; `Network` maps to
widget 421 in `ArnaMain.wnd`; `Host Game` maps to widget 10002 in
`NoxWorld.wnd`; and `New` maps to widget 501 in `SelChar.wnd`. The latter
three controls may expose localized resource keys rather than their visible
English captions, so those root/ID bindings are used as fallbacks. Image-only
controls such as the Warrior portrait require an ID-based wait.

`multiplayerHostMenus` continues from `New` through host setup: Warrior
portrait (class-selection widget 601), class Accept (610), character-name field
(751), character Accept (799), server-name field (10101), and GO (10145). The
tested i386 path did not show a separate `OK` caption after character Accept;
the macro waits for the server-options field instead. That wait also defers
while the `Please wait` modal is visible. The selected character name is
`NOX_CHARACTER_NAME` (default `NoxWarrior`); the server name is
`NOX_SERVER_NAME` (default `NoxDecompServ`).
`multiplayerHostMenusBeforeGo` runs the same flow but stops after filling the
server-name field, which is useful for safe UI verification. The widget IDs are
from `SelClass.wnd`, `SelColor.wnd`, and the server-options window. These are
physical clicks at live widget centers, not direct calls to UI handlers. Before
typing, the macro moves the caret to the end and clears the existing field
value. `type` currently supports ASCII letters, digits, and spaces; unsupported
punctuation is skipped. Accepting the character writes a `.plr` profile under
`Save/`; use a disposable game-data copy or back up existing profiles before
running the macro.

The root pointers are non-owning handles to trees loaded by the menu and
multiplayer setup code. `sub_46C4E0(a1)` receives a window record, marks it for
teardown, removes it from active lists, recursively releases its child/sibling
records, and returns the records to the window pool. Before this happens, the
control-root owner clears any automation handle equal to `a1`; otherwise a
later caption scan can walk freed pool storage after a menu transition. This
was observed after dismissing the welcome/legal window and again after
clicking “Multiplay”. The additional roots are held at `byte_5D4594` offsets
1307736 (class selection), 1308084 (character creation/color), and 1046492
(server options). They are registered by the corresponding UI setup routines
and cleared by the same `sub_46C4E0()` teardown hook as the existing menu roots.

For a front-end-only manual integration probe, run from
`build-deps/gamefiles/app` with control logging enabled:

```sh
ulimit -c unlimited
timeout --signal=TERM --kill-after=3s 150s env \
  ALSOFT_DRIVERS=null LIBGL_ALWAYS_SOFTWARE=1 SDL_VIDEODRIVER=x11 \
  NOX_GAMEPAD=0 NOX_NO_INTERNET_SERVERS=1 NOX_UPNP_ENABLE=0 \
  NOX_CONTROL_SERVER=1 NOX_CONTROL_SERVER_PASSWORD=secret \
  NOX_CONTROL_SERVER_BIND=127.0.0.1 NOX_CONTROL_SERVER_PORT=2323 \
  NOX_CONTROL_SERVER_SLEEP_SCALE=1 NOX_CONTROL_LOG=1 \
  NOX_CHARACTER_NAME=NoxWarrior NOX_SERVER_NAME=NoxDecompServ \
  'NOX_CONTROL_SERVER_BOOT=sleep 5000; macro multiplayerHostMenusBeforeGo;' \
  xvfb-run -a -s '-screen 0 1280x720x24' \
  ../../../build-i386/src/out
```

The game reads `nox.cfg` `VideoMode = 640 480 8` and `Fullscreen = 0` (windowed
640x480x8); Xvfb's 1280x720x24 is only the host display. The documented probe
stops before GO, so it does not start a hosted game. The observed trace does
show server-options initialization reading map resources before GO; it does not
show whether that reads full gameplay state. This was an i386 macro check, not
a 64-bit map test. Do not select or start a map requiring reloaded EUD support
for 64-bit testing. Existing CTest targets do not exercise the live SDL window
tree or physical mouse input, so this front-end check remains an integration
test. The final GO click is part of `multiplayerHostMenus` but is intentionally
omitted from the safe probe.

## Client host-loss lifecycle

`sub_43CCA0()` is the client-side network pump used while joined to a multiplayer
game. It records the last host packet time, shows the connection-wait state after
roughly two seconds without host traffic, and treats the connection as terminal
after the existing 20-second timeout.

A graceful host disconnect reaches `sub_43C860()` as callback message `33`; that
path calls `sub_446380()`, which performs the joined-game teardown/state
transition. A vanished host cannot deliver that callback. The terminal timeout in
`sub_43CCA0()` therefore must also call `sub_446380()` after `sub_43CF70()` marks
the connection-lost state. `sub_43CF70()` is not itself the teardown owner because
it is also used by the quit-menu connection-state path.

The observable contract is that a client must leave the active in-game state
whether the host disconnect notification arrives normally or the host simply
stops responding. Live host-process loss remains an integration/manual check
because the existing focused network tests do not construct the full client game
lifecycle.

## Multiplayer score signedness

The multiplayer score stored at player-info offset `2136` and the corresponding
team score are signed values. Arena suicide and team-kill penalties intentionally
subtract points through `sub_4D8EC0()`, so a score can legitimately become
negative (for example, `0 -> -1`). Packet `0x4E` transports the 32-bit bit pattern
and the player-info join packet restores the compact score as a signed 16-bit
value on the client.

`sub_509A60()` is the server victory check. Outside Elimination/Coop-Team it reads
the configured point limit, scans teams first and then individual players, sets
the game-over flag, and sends the matching winner notification when a score has
reached the limit. Those comparisons must remain signed: reading `-1` through
`_DWORD` turns it into `0xFFFFFFFF`, which falsely satisfies any normal positive
point limit and can make a suicide appear to award the maximum score and end the
match immediately. `tests/multiplayer_score_test.c` drives the production
`sub_509A60()` path for both team and player scores and covers negative and
limit-reaching values.

## Join handshake prerequisite

`sub_438A90()` is the client-side join entry point. Before sending the join
request through `sub_5550A0()`, it calls `sub_420120()` to retrieve the Nox
serial from `HKEY_LOCAL_MACHINE\\SOFTWARE\\Westwood\\Nox`, value `Serial`.
The registry result is the authoritative prerequisite for entering the join
handshake; failure follows the user-facing error path instead of attempting a
join with an uninitialized serial.

`tests/network_join_test.c` drives `sub_420120()` with a deterministic registry
fixture and verifies the production lookup key, value name, type, and serial
payload. The Windows test build selects an explicit test registry seam because
the normal Windows path uses the compatibility registry implementation. Live
server discovery, UDP handshake, and joined-game state remain normal-target
integration coverage.

The public-game listing path uses `nox_parse_games_list_json()` to turn the
lobby `data` array into bounded `nox_server_row` records. Missing port/player
fields receive the game's defaults, while `nox_is_bad_server_ip()` and
`nox_is_bad_server_name()` apply the configured deny lists before fake server
information is queued for the normal discovery path. The regression in
`tests/public_games_test.c` covers valid and incomplete rows, defaults, and
case-insensitive name filtering; live HTTP retrieval and UDP presentation
remain integration coverage.

`sub_5550A0()` is the production client entry point for the initial join
request. It constructs the fixed 100-byte packet with protocol type `14` and
sends it through the active UDP socket to the selected server address and
port. `tests/network_connect_test.c` binds a real loopback UDP server, drives
that entry point, and verifies the request arrives with the expected size and
type. It intentionally stops at the client-send boundary: server-side
handshake acceptance and the subsequent joined-game state transition remain
broader normal-target integration coverage.

When hosting a game start a udp socket on port 18590
Post a registration on the lobby server (if the NOX_LOBBY_REGISTER_ENABLE=1 env variable is set)

which is where the game name comes from

(it then tries to do automatic port opening with upnp if the NOX_UPNP_ENABLE=1 env variable is set)

even if the lobby server or game client can't actually reach the host it will still show the name

then the client sends udp packets to your ip address on the internet
your router needs to either enable upnp or forward those packets to your machine on your LAN

if you have two routers then you need that to happen on both routers

Two router example (might be the case if your router plugs into some kind of other device/modem):
outside router needs to send udp packets for port 18590 to inside router
inside router needs to send packets to computer on network

One router example:
router needs to send packets to computer on network

# Guide

What you're trying to make happen (nox-decomp)
----------------------------------------------

When you **host a game**, nox-decomp:

-   Opens a **UDP** socket on **port 18590**

-   Optionally **registers the game name** on a lobby server if `NOX_LOBBY_REGISTER_ENABLE=1`

    -   Important: **the lobby listing can appear even if your host isn't reachable**

-   Optionally tries **automatic port opening via UPnP** if `NOX_UPNP_ENABLE=1`

For players to actually join you over the internet, your network must allow:

-   **Inbound UDP traffic on port 18590** to reach the hosting PC on your LAN\
    Either via:

    -   **UPnP enabled** on the router (automatic), or

    -   A **manual port forward** (recommended for reliability)

* * * * *

Before you start: get two pieces of info
----------------------------------------

### 1) Find your host PC's local IP (LAN IP)

You need the computer's IP inside your network, usually like `192.168.1.50` or `192.168.0.20`.

-   **Windows:** Settings → Network & Internet → (Wi-Fi/Ethernet) → Properties → "IPv4 address"

-   **Linux/macOS:** look for the `inet` address in network settings, or `ip a` / `ifconfig`

**Tip:** If your PC's IP changes later, the port forward breaks. See "Make it stay fixed" below.

### 2) Know which router is "the internet-facing" one

If you have **one router**, it's simple.

If you have **two routers** (or a router + ISP box that also routes), you likely have a "double NAT" setup. You'll forward on **both**.

* * * * *

Option A: Use UPnP (automatic)
------------------------------

This is the easiest path if it works.

1.  In your router settings, find **UPnP** (often under Advanced / NAT / LAN).

2.  Enable it.

3.  Run nox-decomp with:

    -   `NOX_UPNP_ENABLE=1`

4.  Host a game.

**Notes**

-   UPnP can be flaky depending on routers/firewalls.

-   Some setups disable UPnP by policy (common in managed/ISP equipment).

If UPnP doesn't work or you want consistent results, use manual forwarding.

* * * * *

Option B: Manual port forwarding (recommended)
----------------------------------------------

You will create a rule:\
**Forward UDP port 18590 (WAN) → your host PC's LAN IP port 18590**

### Step 1: Log in to your router

1.  Open your router admin page in a browser (common addresses):

    -   `192.168.0.1`

    -   `192.168.1.1`

    -   `192.168.1.254`

2.  Log in with your admin credentials (often printed on the router or set by you).

### Step 2: Find Port Forwarding / Virtual Server / NAT

Look for a menu like:

-   **Port Forwarding**

-   **Virtual Server**

-   **NAT**

-   **Applications & Gaming**

-   **Firewall / NAT Rules**

### Step 3: Add a new rule

Create a rule with values like:

-   **Name/Description:** `nox-decomp`

-   **Protocol:** `UDP`\
    (If the router forces "TCP/UDP" as one option, you *can* select it, but UDP is the important part.)

-   **External/WAN Port:** `18590`

-   **Internal/LAN IP:** *(your host PC's LAN IP, e.g. `192.168.1.50`)*

-   **Internal Port:** `18590`

-   **Enable:** Yes

Save/apply.

### Step 4: Allow it through the host PC firewall

Your OS firewall must allow **inbound UDP 18590**.

-   **Windows Defender Firewall:** allow the app, or add an inbound rule for UDP 18590

-   **Linux:** allow UDP 18590 in ufw/firewalld/nftables as appropriate

* * * * *

If you have TWO routers (double NAT): forward on both
-----------------------------------------------------

This is the most common "it's listed but nobody can join" cause.

### Identify the topology

-   **Outside router** (internet-facing): gets the public internet connection

-   **Inside router**: your PC is connected here

### Step 1: Forward from outside router → inside router

On the **outside router**, forward:

-   **UDP 18590** → **inside router's WAN IP**

To do that, you need the **inside router's WAN/Internet IP** as seen by the outside router (often something like `192.168.0.2`).

### Step 2: Forward from inside router → your PC

On the **inside router**, forward:

-   **UDP 18590** → **your PC's LAN IP**

### Better alternative if you can:

-   Put the inside router into **Access Point mode** (so you only have one NAT/router)

-   Or set the outside router to **bridge/modem mode** (depends on ISP hardware)

-   Or put the inside router in the outside router's **DMZ** (less ideal, but sometimes simplest)

* * * * *

Make it stay fixed (avoid breakage)
-----------------------------------

Port forwarding targets an IP. If your PC gets a new LAN IP, your forward points at the wrong device.

Do one of these:

-   **DHCP Reservation** on the router (best): "Always give this device the same IP"

-   **Static IP on the PC** (works, but make sure it's outside the DHCP pool or configured carefully)

* * * * *

Quick checklist when players can't connect
------------------------------------------

-   ✅ You forwarded **UDP** (not just TCP)

-   ✅ Forward points to the **correct PC LAN IP**

-   ✅ PC LAN IP didn't change

-   ✅ Host firewall allows **UDP 18590**

-   ✅ You don't have **two routers** (or you forwarded on both)

-   ✅ UPnP isn't "enabled" but blocked by another setting (some routers have both UPnP + NAT-PMP toggles, or "secure UPnP")

* * * * *

What to tell players / what to expect
-------------------------------------

-   Players join by sending **UDP packets to your public IP** on **port 18590**

-   The lobby name can still show even if you're unreachable (registration ≠ connectivity)

-   Once forwarding/UPnP is correct, they should be able to connect reliably

# Alternatives 

If you all know each other and just want to play without opening UDP ports on the router, the most popular "safe enough and low-friction" options are **private overlay networks** (basically: pretend everyone is on the same LAN).

1) Mesh VPN apps (easy, popular)
--------------------------------

These create a private network between your PCs, so the game can use LAN-style traffic.

### **Tailscale (WireGuard-based)**

-   Very popular, simple UI, generally reliable through NAT (often works without port forwards).

-   Good security model (modern crypto, device auth, easy to revoke a device).

-   "Tailnet" can be restricted to just your friends.

-   If direct connections fail, it may relay via DERP (works but can add latency).

### **ZeroTier**

-   Also very popular for "virtual LAN" gaming.

-   Usually straightforward: join a network ID, authorize members.

-   Great for games that behave well on LAN, and often works even under tricky NATs.

**Why these are good:** minimal router changes, easy membership control, and you can keep the game bind on a private IP.

* * * * *

2) Roll your own WireGuard (most control, more setup)
-----------------------------------------------------

If one person *can* host a small always-on node (a VPS, home server, or a friend with a friendly router), then everyone connects to it.

-   **Pros:** very fast, very secure, no third-party coordination layer beyond your server.

-   **Cons:** more setup and key management; if you use a VPS you're still "opening" something, but it's **WireGuard** (single UDP port) rather than the game port.

This is the "best-practice" option if someone in the group is comfortable with networking.

* * * * *

3) "Host a relay server" approach (no inbound to players, but needs infra)
--------------------------------------------------------------------------

Instead of players connecting to a home host directly, everyone connects *outbound* to a public server that relays traffic.

-   **Pros:** zero router config for anyone at home.

-   **Cons:** you need a server + bandwidth; latency might be higher; you'd need game support or a proxy method.

For friend groups, this is usually overkill unless you already run a VPS or can host it on your internal network and open up a port on your router.

Here are docker versions of nox-decomp to help
[docker-compose.yml](https://github.com/sookyboo/nox-decomp/blob/main/dist-scripts/docker-compose.yml)

[kubernetes](https://github.com/sookyboo/nox-decomp/blob/main/dist-scripts/nox-decomp-kube.yml)
