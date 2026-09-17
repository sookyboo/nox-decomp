The network flow for nox-decomp is something like this

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

## Native bot lifecycle diagnostic and experimental spawn

With `USE_BOT_SUPPORT=ON`, the server can compare normal network player joins
with the experimental socketless bot lifecycle. Diagnostics are disabled by
default. Enable them with `NOX_BOT_LIFECYCLE_TRACE=1` before launch or with
`bot trace on` from the authoritative server console. Every line is written to
`stderr` with the `[bot-lifecycle]` prefix and `path=` / `phase=` fields.

The normal network path traces the incoming join/leave boundary plus checkpoints
inside `sub_4DD320`: object creation, player-info activation, runtime linkage,
spawn placement, and final join result. The leave trace surrounds the normal
`sub_4DE7C0(slot)` owner.

The bot build also exposes a full experimental server-created lifecycle:

```text
bot spawn auto warrior hardcore
bot spawn red wizard normal
bot spawn blue conjurer hard
bot spawn 3v3 normal
bot clear <slot>
bot clear all
```

`bot spawn` selects an inactive player-info slot in `0..30`, constructs the
recovered 153-byte `PlayerOpts` shape, calls the normal native player constructor,
and then activates the original player-monster bot update path. Explicit red/blue
spawns now resolve that existing native team before construction and seed its
external team key into the recovered `PlayerOpts` field; membership is verified
after construction as a fallback. A missing requested team is rejected before a
player object is created. `auto` leaves native mode/team selection untouched.

Spawned names also follow Bot-Script: no-team bots are `Lance`, `Kirik`, and
`Horst`; team games use `Warrior Bot`, `Wizard Bot`, and `Conjurer Bot`. Native
duplicate-name handling remains authoritative. `bot clear` only removes slots
created by the spawn command and reuses the normal leave owner.

This is intentionally a runtime-verification attempt. It has not yet established
that every network send made by `sub_4DD320` is harmless for a slot with no real
peer or that `sub_4DE7C0` leaves no socketless slot state behind. The recovered
pre-constructor network admission code mixes live-peer capacity with password,
account, spectator, ping, disabled-class, and team-creation policy, so it is not
replayed as a generic bot-capacity check. Capture the trace from one real join
plus one bot spawn/clear and compare phases before adding lifecycle writes outside
those native owners.
