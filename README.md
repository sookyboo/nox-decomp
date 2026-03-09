![](https://github.com/sookyboo/nox-decomp/blob/main/logos/nox-decomp.png)
# Nox-Decomp

The game is fully playable on [PortMaster](https://portmaster.games/detail.html?name=nox-decomp), SteamDeck with flatpack, Linux and Windows for both single player and multiplayer and all classes warrior, wizard and conjurer!

Nox-Decomp requires an original copy of Nox. None of the Nox game assets are provided by this project. To get a legitimate copy of the game assets, please refer to the [GoG release of Nox](https://www.gog.com/game/nox).

The dialog audio files need to be converted in order for the dialog to work. See below for details. Many of the releases include scripts to automatically convert.

If you enjoy this please consider giving me a tip on [![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/Y8Y61U19WK)

This is a fork of https://github.com/neuromancer/nox-decomp this fork fixes all the bugs, adds movie, LAN, Internet (via OpenNox lobby servers) support and compiles for ARMHF devices.

Other notable projects:
[Opennox](https://github.com/opennox/opennox/tree/v1.9.0-alpha13) - This was difficult to get working on 32bit ARM and has bugs which is why this fork started

Why is this different than Opennox:
- This fork has fully complete-able single player campaigns and the server version can host any style of game without crashing.
- Built-in gamepad support.
- Works on any native resolution of a device and scales the original game resolution to the device native resolution. 
- Can join and host public internet games listed on the OpenNox lobby servers. 
- Works on $50 ARM linux based handheld game consoles on [PortMaster](https://portmaster.games/detail.html?name=nox-decomp).
- Fast and very close to the original game. 
- 32bit currently but possibly a 64bit version in future. (The way the code accesses memory expects 32bit and most likely needs changes in thousands of locations)

Project Goals and progress:

(This is the order goals are prioritised)

- Fix any seg faults on armhf and i386 - progress 100% (game stable - no known seg faults - all solo wizard/conjurer/warrior play tests completed)
- Easy setup for new players - progress 99% (launchers that automatically extract game and convert audio for linux, portmaster, windows and steamdeck flatpak. Windows launcher sometimes fails to launch game)
- Fix any networking issues - progress 100% (no known issues)
- Fix any known rendering issues - progress 99% (no known rendering issues)
- Fix any known audio issues - progress 97% (minor noisy glitch between song changes, auto dialog conversion required)
- Gamepad support - progress 98% (emulates mouse and keyboard, config file, no direct player movement yet, thumb sticks might be too sensitive and skip dialog screens between chapters)
- Try and port to 64bit - progress 2% (have planned two possible options forward but requires many changes)
- New features
  - opennox lobby integration works but no hd support for lobby
  - control server that emulates mouse and keyboard clicks for server mode
  - 1024x768ish resolution is automatically scaled to hd displays when using launcher without changing aspect ratio
  - limit mouse movement for gamepad compatibility - toggleable
  - HD might be possible by directly editing nox.cfg and launcher but untested
- Rewrite game functions to be easily readable

# Screenshots

![](https://github.com/sookyboo/nox-decomp/blob/main/screenshots/game1.png)
![](https://github.com/sookyboo/nox-decomp/blob/main/screenshots/handheld.jpeg)
![](https://github.com/sookyboo/nox-decomp/blob/main/screenshots/movie1.png)
![](https://github.com/sookyboo/nox-decomp/blob/main/screenshots/movie2.png)

# Thanks
Thanks to my wife for giving me the time to work on this.

Thanks to my brother for playing this game with me when I was growing up.

Thanks to OpenNox for implementing a modern lobby server which enables internet games and for labeling functions in their code which made navigating this code easier.

Thanks to klops, JanTrueno, Dia, Fraxinus88, OGM, Ganimoth, NotYerAvgPorter, Hades-Arcadius, MrGiKILL and Cebion from the [PortMaster discord](https://discord.com/invite/JxYBp9HTAY) for contributing fixes, advice and testing for the PortMaster port.

Thanks to szhublox and Lovyxia from the [NoX RPG Unofficial discord](https://discord.gg/4bYwu68) for testing, good suggestions and knowing the original game so well.
Thanks to szhublox for finding the original decompiled binary version 1.2b with sha256: e6e1b56029f8871c25d0caf8bcedf7bf1699580d0dc24c90d79eb27e9d7e05b2
Thanks to szhublox for pairing on obliterate rendering fixes, finding where the spell renders and uncovering a hint of where to look for a fix.
Thanks to szhublox for videos of force of nature, obliterate and mana drain problems.
Thanks to szhublox for fixing rendering for Ring of Fire spell.

# Running the game

## PortMaster
Open the PortMaster app on your device and install NoxDecomp and follow the readme on [PortMaster](https://portmaster.games/detail.html?name=nox-decomp)

The file is also included in releases here.

## Steam Deck
Please note the flatpak requirements:
- It  needs access to your devices for gamepad support (all flatpaks do for gamepad support until flatpak 1.16 is released on steam deck) .
- It requires access to your steam config folders to add a launcher for steam
- It requires a copy of `setup_nox_2.0.0.20.exe` from gog in ~/nox-decomp/gamesfiles

Go to Power -> Switch to Desktop

Controlling the mouse in Steam Deck desktop mode: 
- Right touch pad lets you move the mouse
- R2 is click and L2 is right click
- Steam button + X shows the on screen keyboard (you can use the touch screen to enter text)

[Video Guide](https://youtu.be/vSfgxl9E7i4)

Log in to your [gog.com](https://www.gog.com/en/game/nox) account and buy/download nox the setup file should be something like `setup_nox_2.0.0.20.exe`

Download the flatpak.zip from [releases section](https://github.com/sookyboo/nox-decomp/releases) in the chrome browser on the steam deck.

Open up the file manager (File Manager is called Dolphin)

Go to Downloads

Double Click flatpak.zip

Open it up in Ark

Click the Extract button in the top left

Extract to the Downloads folder (should be already there)

Click Extract

Back in the file manager (Dolphin) there should be a flatpak directory go inside it

Right click the steam icon in the system tray and click Exit Steam

Double click install.sh

Choose execute

Wait while it installs the flatpak and its dependencies.

In the pop up click "Run" to run nox-decomp now and start the setup extraction process.

It will ask to add a launcher to your steam deck. Click "Yes"

When the steam launcher step is finished click "OK"

Read the instructions for installing the game data and click "OK"

Choose "select GOG installer" and click "OK"

Go to your Downloads and select `setup_nox_2.0.0.20.exe` and click "OK"

Wait for the extraction and conversion process to finish.

Note: You will only see it in your steam library the next time steam restarts.

Restart your device or launch the game from the start menu and enjoy!

## Linux
Log in to your [gog.com](https://www.gog.com/en/game/nox) account and buy/download nox the setup file should be something like `setup_nox_2.0.0.20.exe`

From [releases section](https://github.com/sookyboo/nox-decomp/releases) :

Download nox-decomp-linux-armhf.tar.gz for arm64/armhf devices
Download nox-decomp-linux-i386.tar.gz for intel i386/32bit/amd64 devices

The following steps were made with Ubuntu 24.04 and vary depending on which Linux you are running but will be very similar.

Right click your nox-decomp tar.gz file downloaded above in your downloads folder and choose Extract

This should create a folder with the name nox-decomp-linux-i386 or nox-decomp-linux-armhf

Go inside the folder

Right click the Nox-Decomp.sh file and choose Run as Program

Click OK when it says Nox game data not found .

Navigate to your downloads where you have `setup_nox_2.0.0.20.exe`

(If you don't get a file chooser you can manually copy `setup_nox_2.0.0.20.exe` into the gamefiles directory.)


It will ask to add a launcher to your steam installation. Click "Yes" or "No" depending on if you want a shortcut in steam or not.
Note: You will only see it in your steam library the next time steam restarts.

When the steam launcher step is finished click "OK"

Wait for the extraction and conversion process to finish.

Click "OK" and you should enter the game.


## Windows
Download the zip from releases.

Buy the game from here [GoG Nox](https://www.gog.com/en/game/nox)

Unpack nox-decomp and copy the gog setup program into gamefiles.

Run the included Nox-Decomp.bat script which expects the nox installation file in the nox-decomp `gamefiles` directory.

After extraction it should start the game.

Can take up to a minute on a fast computer.

## Mac
This works with crossover (macos wine can't run 32bit binaries).

Download the windows zip from releases.

Buy the game from here [GoG Nox](https://www.gog.com/en/game/nox)

Unpack nox-decomp and copy the gog setup program into gamefiles.

Run the included Nox-Decomp.bat script which expects the nox installation file in the nox-decomp `gamefiles` directory.

Make a crossover bottle. Right click hit option open a shell and navigate to your directory with nox.

Run Nox-Decomp.bat inside the nox directory with `wine NoxDecomp.bat`.

Intel Macs:
Building for Intel Macs should be possible but that won't run on m series macs - rosetta doesn't support 32bit binaries

M series macs:
M series macs can't run 32bit arm binaries

## Server
There are docker arm64 and amd64 images that run the 32bit version

Example kubernetes and docker-compose files:

[docker-compose.yml](https://github.com/sookyboo/nox-decomp/blob/main/dist-scripts/docker-compose.yml)

[kubernetes](https://github.com/sookyboo/nox-decomp/blob/main/dist-scripts/nox-decomp-kube.yml)


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

# NOX_LIMIT_RANGE_ON_RUN_GAMEPAD - useful for gamepads and steam deck 
# limits the range of the mouse when running but only if starting close to center or passing through center
export NOX_LIMIT_RANGE_ON_RUN_MOUSE=0 #default is 0
export NOX_LIMIT_RANGE_ON_RUN_GAMEPAD=1 #default is 1
export NOX_LIMIT_RANGE_ON_RUN_RADIUS=110 # default is 110 - the radius of the circle

# Built in gamepad support
export NOX_GAMEPAD=1
export NOX_GAMEPAD_INI="$PWD/nox.gptk2.ini" # Mapping file based on gptokeyb2 must be present to work

export NOX_GAMEPAD_EXIT=1 # when pressing start and select exit game 

export NOX_GAMEPAD_AUTOSWAP_XBOX=1 # swap A and B automatically for xbox/nintendo controllers 
export NOX_GAMEPAD_FLIP_ABXY=0 # manually swap A and B buttons
export NOX_GAMEPAD_LOG=0 # for debbuging gamepad issues    

export NOX_LINEAR_SCALING=1 # when scaling don't keep things pixel perfect but apply smoothing
export NOX_INTEGER_SCALING=0 # only scale to the highest integer value that fits in the screen don't use floats to fit exactly on screen
```

# Known issues
The game is fully playable on PortMaster, SteamDeck with flatpack, Linux and Windows for both single player and multiplayer and all classes warrior, wizard and conjurer!

* Minor - 16-bit graphics work perfectly but 8-bit doesn't work
* Minor - all text is legible - but there might be distortion based on your scaling settings and resolution enabling smoothing with NOX_LINEAR_SCALING on small consoles may fix.
* Minor - On low power devices fade ins and fade outs are slightly slower

# Building from source

See [build.sh](https://github.com/sookyboo/nox-decomp/blob/main/build.sh) and [lightbuild.sh](https://github.com/sookyboo/nox-decomp/blob/main/dist-scripts/lightbuild.sh)

```
cd build
cmake ..
cmake --build . -j $(nproc)
```

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

Are you the one that should be mentioned here? Let me know I will add your name.

Neuromancers original comments:
I am not the original author of this code. It comes from the [playnox.xyz](https://playnox.xyz) website. A copy of the original source code is permanently archived [here](https://web.archive.org/web/20191104220905/https://playnox.xyz/public_v1.zip).

# Legal
Nox-Decomp is released to the Public Domain. The documentation and function provided by Nox-Decomp may only be utilized with assets provided by ownership of Nox.

Nox™ (C) 2000 Electronic Arts Inc.  All rights reserved. Nox are trademarks or registered trademarks of Electronic Arts in the U.S. and/or other countries.

Nox-Decomp and any of it's maintainers are in no way associated with or endorsed by Electronic Arts.
