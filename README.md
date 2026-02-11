![](https://github.com/sookyboo/nox-decomp/blob/main/logos/nox-decomp.png)
# Nox-Decomp

If you enjoy this please consider giving me a tip on [![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/Y8Y61U19WK)

This is a fork of https://github.com/neuromancer/nox-decomp

Fixed most of the known bugs for Neuromancer's version and adds movie and LAN support and compiles for ARMHF devices.

Also adds support for joining internet games from OpenNox lobby servers.

A build is or will be available for [portmaster](https://portmaster.games/).

Other notable projects:
[Opennox](https://github.com/opennox/opennox/tree/v1.9.0-alpha13) - This was difficult to get working on 32bit ARM which is why this fork started

Why is this different than Opennox:
This fork has fully complete-able single player campaigns and the server version can host any style of game without crashing.
The releases also have gamepad support.
It can join and host public internet games listed on the Opennox lobby servers. 
It also works on $50 ARM based game consoles see the port master version below.
It is fast and very close to the original game. It is still 32bit.

Neuromancers original comments:
**The public development of a Nox reimplementation was moved to the [Nox Discord server](https://discord.gg/4bYwu68). Feel free to join to follow the development of this project.**

I am not the author of this code. It comes from the [playnox.xyz](https://playnox.xyz) website. A copy of the original source code is permanently archived [here](https://web.archive.org/web/20191104220905/https://playnox.xyz/public_v1.zip). I just did some small modifications to properly run it in Linux.

Nox-Decomp requires an original copy of Nox. None of the Nox game assets are provided by this project. To get a legitimate copy of the game assets, please refer to the [GoG release of Nox](https://www.gog.com/game/nox).

# Screenshots

![](https://github.com/sookyboo/nox-decomp/blob/main/screenshots/game1.png)
![](https://github.com/sookyboo/nox-decomp/blob/main/screenshots/handheld.jpeg)
![](https://github.com/sookyboo/nox-decomp/blob/main/screenshots/movie1.png)
![](https://github.com/sookyboo/nox-decomp/blob/main/screenshots/movie2.png)

# Building from source

```
cd build
cmake ..
cmake --build . -j $(nproc)
```

# Running the game

```
innoextract setup_nox_2.0.0.20.exe
mv app/* .
./src/out
```

# Converting dialog audio files
The audio dialog files need to be converted in order for them to work

```bash
  # example can be found here: https://github.com/sookyboo/PortMaster-New/blob/sookyboo_nox_decomp/ports/nox-decomp/Nox-Decomp.sh
  export DIALOG_DIR="Dialog" # Put a full path to your extracted Dialog directory
  export FFMPEG_BIN="ffmpeg"
  shopt -s nullglob nocaseglob
  wav_files=("$DIALOG_DIR"/*.wav)
  total="${#wav_files[@]}"

  if [ "$total" -eq 0 ]; then
    echo "No dialog WAV files found, skipping conversion"
    return 0
  fi

  echo "Converting dialog audio ($total files)"
  sleep 1

  i=0

  # -------------------------------------------------
  # Convert with progress updates
  # -------------------------------------------------
  for wav in "${wav_files[@]}"; do
    tmp="${wav}.tmp"

    if "$FFMPEG_BIN" -y \
        -loglevel error \
        -i "$wav" \
        -ac 1 \
        -ar 22050 \
        -c:a pcm_s16le \
        -f wav \
        "$tmp"; then
      mv "$tmp" "$wav"
    else
      rm -f "$tmp"
      echo "ERROR converting $(basename "$wav")"
      return 1
    fi

    i=$((i + 1))
  done

```

# Internet Lobby env vars 
These are the default assumed values for these env vars if not present:
```
export NOX_NO_INTERNET_SERVERS=0 # 1 switches off internet access but keeps LAN access, 0 allows internet access

export NOX_LOBBY_HOST=nox.nwca.xyz
export NOX_LOBBY_PORT=8088
export NOX_LOBBY_PATH="/api/v0/games/list"

export NOX_LOBBY_CONNECT_TIMEOUT=2000 # in milliseconds
export NOX_SERVER_CACHE_TTL=30 # How long to cache internet game queries - minimum 30 seconds

# If there are bad servers that crash the game they can be filtered using this list
export NOX_BAD_SERVER_IPS="127.1.1.1,127.1.1.2"
export NOX_BAD_SERVER_NAMES="VeryBadServerName1,VeryBadServerName2"

# these register the game on opennox lobby
export NOX_LOBBY_REGISTER_ENABLE=0
export NOX_LOBBY_REGISTER_PERIOD=20
export NOX_LOBBY_REGISTER_PATH=/api/v0/games/register
export NOX_SERVER_MODE=ctf
export NOX_SERVER_VERS=1.2

# this is to automatically open udp port 18590 when hosting games on a router that supports UPNP
export NOX_UPNP_ENABLE=0
export NOX_UPNP_DEBUG=0
export NOX_UPNP_PORT=18590
export NOX_UPNP_PROTO=udp
export NOX_UPNP_TIMEOUT_MS=2000
```

# Control server and env vars
The control server allows you to control nox with mouse clicks and keyboard presses
It is useful for testing and also starting multiplayer games in an automated way.

```
export NOX_CONTROL_SERVER=1
export NOX_CONTROL_SERVER_PASSWORD=secret
export NOX_CONTROL_SERVER_BIND=127.0.0.1
export NOX_CONTROL_SERVER_PORT=2323

export NOX_SKIP_INTRO_MOVIES=1 # useful if issuing commands at boot
export NOX_CONTROL_SERVER_SLEEP_SCALE=1 # some ennvironments might be slow so you can increase the sleep time between commands 
export NOX_CONTROL_SERVER_BOOT="macro server;" # You can issue control server commands on start

# The macro server uses some env vars and sets up a multiplayer game
export NOX_SERVER_NAME=NoxDecompServer # when starting a game what the server is called 
export NOX_SERVER_SYSOP=secret # set the sysop password to secret for multiplayer games
export NOX_SERVER_LESSONS=15
export NOX_SERVER_TIME:0
export NOX_SERVER_DEFAULT_MAP:capflag # game type becomes whatever the map default is

export NOX_CAPTURE_INPUT=0   # prints out real user mouse input but mostly useless too noisy 
```

# Other env vars
```
export NOX_SKIP_INTRO_MOVIES=0 # default is 1 - skip the logo movies at the start of the game

# NOX_LIMIT_RANGE_ON_RUN - useful for gamepads and steam deck 
# limits the range of the mouse when running but only if starting close to center or passing through center
export NOX_LIMIT_RANGE_ON_RUN=1 #default is 0
export NOX_LIMIT_RANGE_ON_RUN_RADIUS=110 # default is 110 - the radius of the circle   
```

# Steam deck 
Download the flatpak from releases section.

It requries access to a directory in your home directory called nox-decomp

It also needs access to your devices to emulate the mouse and keyboard ( a future release might remove this requirement).


Dependencies:
```
flatpak install --user -y flathub \
org.freedesktop.Platform//24.08 \
org.freedesktop.Platform.Compat.i386//24.08 \
org.freedesktop.Platform.GL32.default//24.08
```

Install:
```
# assuming io.github.sookyboo.NoxDecomp.flatpak is in the current dir
flatpak install -y --user --reinstall io.github.sookyboo.NoxDecomp.flatpak
```

Create a launcher for steam:

Using Dolphin file browser

Go to Home/.local/share/flatpak/exports/share/applications

Right click `io.github.sookyboo.NoxDecomp.desktop`

Click Add to Steam


Setup Controller (These instructions may differ as Valve changes steam):

Get out of desktop mode by restarting

Go to your library, Non-Steam collection

Click NoxDecomp

Click Controller settings button

Click using template:

Under Layouts created by Valve:

Choose Gamepad with Mouse Trackpad

Press X to apply the layout

# Windows

Download the zip from releases.

Unpack it and copy the files into your nox directory. 

You can get the nox files by using [innoextract](https://github.com/dscharrer/innoextract/releases) and buying the game from here [GoG Nox](https://www.gog.com/en/game/nox)


Run noxd.exe inside the nox directory.

(Source code is available in the windows branch for now)

# Mac
This works with crossover/wine.

Download the zip from releases.

Unpack it and copy the files into your nox directory.

You can get the nox files by using [innoextract](https://github.com/dscharrer/innoextract/releases) and buying the game from here (GoG Nox)[https://www.gog.com/en/game/nox]

Make a crossover bottle. Right click hit option open a shell and navigate to your directory with nox.

Run noxd.exe inside the nox directory with `wine noxd.exe`.

# PortMaster
Open the PortMaster app on your device and install NoxDecomp and follow the readme on [PortMaster](https://portmaster.games/games.html)

The file is also included in releases here. 

# Server
There are docker arm64 and amd64 images that run the 32bit version
```
amd64:
docker run --pull always --rm --name nox-decomp-server --platform linux/amd64 -p 18590:18590/udp -v \${PWD}/gamefiles:/opt/nox-decomp/gamefiles ghcr.io/nox-decomp/nox-decomp-server:latest

arm64:
docker run --pull always --rm --name nox-decomp-server --platform linux/arm64 -p 18590:18590/udp -v \${PWD}/gamefiles:/opt/nox-decomp/gamefiles ghcr.io/nox-decomp/nox-decomp-server:latest

if your device doesn't have 32bit cpu support you will need docker qemu (e.g. mac m1):
docker run --privileged --rm tonistiigi/binfmt --install all

You will need to set env vars and port forward from your router to the container host on port 18590 UDP

If you want to register the game please make sure port forwarding is working and then:
NOX_LOBBY_REGISTER_ENABLE=1
Only register if port forwarding is enabled to reduce unusable servers in the list.

I don't think you can broadcast udp from a docker container so you can't find this on a local lan only on internet with register enabled.

The container runs as user 1001 for security and you need a copy of the game to run it.

You can place the gog game installer in gamefiles dir and it will automatically extract it.
```

# Known issues

* Minor - all graphics are totally corrupted in 8-bit color mode but work in 16-bit color mode (16bit is better anyway).
* Minor glitch on the last column of pixels on text only on some characters - barely noticeable - all text is legible.
* Minor glitch on death rays appearing in shadows when they should be hidden from view.
* Some slow downs in fade outs and fade ins and not noticeable on powerful hardware. (This is due to the way that the game is being drawn and was present in the original game I think)

# License

Regarding this code, the author [indicated that](https://www.reddit.com/r/linux_gaming/comments/cknh3l/play_nox_2000_in_a_browser_opensource_but/evrnrjh/):

> I would not consider this to be FOSS (free and open-source software). My goal was to show that this type of effort is now possible with modern tools. Also, for context, Nox has been abandoned by its creators and only runs on modern Windows thanks to community patches.

Following the [devilution](https://github.com/diasurgical/devilution) project, I think Public Domain is the best license for this.

# F.A.Q.

> Wow, does this mean I can download and play Nox for free now?

No, you'll need access to the data from the original game. If you don't have an original CD then you can [buy Nox from GoG.com](https://www.gog.com/game/nox). 

> Is Nox-Decomp even legal?

That's a tricky question. Under the DMCA, reverse-engineering has exceptions for the purpose of documentation and interoperability. Nox-Decomp provides the necessary documentation needed to achieve the latter. However, it falls into an entirely gray area. The real question is whether or not  Electronic Arts deems it necessary to take action.

# Credits
- Westwood Studios
- [/u/awesie](https://www.reddit.com/u/awesie)
- neuromancer (for some Linux fixes)
- Sookyboo (for fixing 16bit cursor color, solo game, spell rendering fixes, arm32bit crashes, adding video support and internet game support)

Are you the one that should be mentioned here? Let me know I will add your name. Also, if you are interested in continue this project, I can give you administrative access to this repository.

# Legal
Nox-Decomp is released to the Public Domain. The documentation and function provided by Nox-Decomp may only be utilized with assets provided by ownership of Nox.

Nox™ (C) 2000 Electronic Arts Inc.  All rights reserved. Nox are trademarks or registered trademarks of Electronic Arts in the U.S. and/or other countries.

Nox-Comp and any of its' maintainers are in no way associated with or endorsed by Electronic Arts.
