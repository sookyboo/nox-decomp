The network flow for nox-decomp is something like this

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