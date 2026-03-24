#!/bin/bash

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

source "$controlfolder/control.txt"

# Fixes an issue with pipewire on some machines and possibly other fixes
export PORT_32BIT="Y"

# We source custom mod files from the portmaster folder example mod_jelos.txt which containts pipewire fixes
[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"

get_controls

# 1) Default: run arch follows device arch
RUN_ARCH="${DEVICE_ARCH}"

# 2) Remap device arch -> desired run arch
case "${RUN_ARCH}" in
  aarch64|arm64)
    RUN_ARCH="armhf"
    ;;
  amd64|x86_64)
    RUN_ARCH="i386"   # 32-bit x86 userspace
    ;;
esac

# ---------------------------
# Paths
# ---------------------------
GAMEDIR="/$directory/ports/nox-decomp"
CONF_DIR="$GAMEDIR/conf"
ASSET_DIR="$GAMEDIR/gamefiles/app"
SAVE_DIR="$ASSET_DIR/Save"
GPTK_CFG="nox.gptk2.ini"
BINARY="noxd"
UTILDIR="$GAMEDIR/utils"

INNOEXTRACT="$controlfolder/innoextract.$DEVICE_ARCH"
DATADIR="$GAMEDIR/data"
INSTALLER_EXE_GLOB="setup_nox*.exe"
# ---------------------------
# Logging
# ---------------------------
mkdir -p "$GAMEDIR"
> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

# ---------------------------
# Create config/save dir
# ---------------------------
$ESUDO mkdir -p "$CONF_DIR/Save"
$ESUDO mkdir -p "$SAVE_DIR"
bind_directories "$SAVE_DIR" "$CONF_DIR/Save"

if [ ! -d "$ASSET_DIR" ]; then
  pm_message "Nox game assets not found in gamefiles/"
  # exit 1
fi

install() {
  # -------------------------------------------------
  # Locate source data
  # -------------------------------------------------
  SRC="$GAMEDIR/gamefiles"

  # -------------------------------------------------
  # Run extractor if needed
  # -------------------------------------------------
  NEEDED="$SRC/app/gamedata.bin"

  if [ ! -f "$NEEDED" ]; then
    pm_message "Nox data not extracted"
    sleep 1

    if [ -z "$SRC" ]; then
      pm_message "Put Nox files in:"
      pm_message "$GAMEDIR/gamefiles/"
      sleep 5
      exit 1
    fi

    mkdir -p "$DATADIR"

    found_installer="no"
    for file in "$SRC"/$INSTALLER_EXE_GLOB; do
        if [ -f "$file" ]; then
            found_installer="yes"
            break
        fi
    done

    if [ "$found_installer" = "yes" ]; then
        pm_message "Found Nox GOG installer"
        pm_message "Part 1 is extracting (10 min) and Part 2 is converting audio files (20min)"
        pm_message "Part 1 of 2 - Extracting Nox GOG installer..."
        sleep 1
        "$INNOEXTRACT" "$SRC"/$INSTALLER_EXE_GLOB -d "$SRC"
    fi

    if [ ! -f "$NEEDED" ]; then
      pm_message "Extraction failed"
      pm_message "Put Nox files in:"
      pm_message "$GAMEDIR/gamefiles/"
      sleep 5
      exit 1
    fi

    pm_message "Extraction complete"
    sleep 1

    pm_message "Delete installer nox.cfg."
    rm "$GAMEDIR/gamefiles/app/nox.cfg" || true

    pm_message "Delete installer files."
    rm -fR "$SRC"/$INSTALLER_EXE_GLOB
    sleep 1
  fi
}

convert_dialog() {
  MARKER_FILE="$ASSET_DIR/converted_dialog.txt"
  DIALOG_DIR="$ASSET_DIR/Dialog"
  FFMPEG_BIN="$UTILDIR/ffmpeg.${DEVICE_ARCH}"

  # -------------------------------------------------
  # Skip if already converted
  # -------------------------------------------------
  if [ -f "$MARKER_FILE" ]; then
    return 0
  fi

  # -------------------------------------------------
  # Skip on 32-bit systems (ffmpeg is 64-bit only)
  # -------------------------------------------------
  if [ "$(getconf LONG_BIT)" = "32" ]; then
    pm_message "32-bit system detected, skipping dialog audio conversion"
    return 0
  fi

  # -------------------------------------------------
  # Only run if game data exists
  # -------------------------------------------------
  if [ ! -f "$NEEDED" ]; then
    pm_message "Game data not present, skipping dialog conversion"
    return 0
  fi

  # -------------------------------------------------
  # Preconditions
  # -------------------------------------------------
  if [ ! -x "$FFMPEG_BIN" ]; then
    pm_message "ERROR: ffmpeg not found at $FFMPEG_BIN"
    return 1
  fi

  if [ ! -d "$DIALOG_DIR" ]; then
    pm_message "ERROR: Dialog directory not found"
    return 1
  fi

  # -------------------------------------------------
  # Gather WAV files
  # -------------------------------------------------
  shopt -s nullglob nocaseglob
  wav_files=("$DIALOG_DIR"/*.wav)
  total="${#wav_files[@]}"

  if [ "$total" -eq 0 ]; then
    pm_message "No dialog WAV files found, skipping conversion"
    return 0
  fi

  pm_message "Part 2 of 2 - Converting dialog audio ($total files)"
  sleep 1

  i=0
  PortMasterDialog "progress" "message" "$i" "$total"

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
      PortMasterDialog "progress_clear"
      pm_message "ERROR converting $(basename "$wav")"
      return 1
    fi

    i=$((i + 1))
    PortMasterDialog "progress" "Converting dialog audio" "$i" "$total"
  done

  # -------------------------------------------------
  # Finish up
  # -------------------------------------------------
  PortMasterDialog "progress_clear"
  echo "Dialog audio converted to PCM on $(date)" > "$MARKER_FILE"

  pm_message "Dialog audio conversion complete"
  sleep 1
}

# -------------------------------------------------
# Install game data
# -------------------------------------------------
install
#convert_dialog # game should not need conversion of audio files anymore

# ---------------------------
# Runtime environment
# ---------------------------
cd "$ASSET_DIR"

export XDG_DATA_HOME="$CONF_DIR"

# Setup gl4es
if [ -f "${controlfolder}/libgl_${CFW_NAME}.txt" ]; then
  source "${controlfolder}/libgl_${CFW_NAME}.txt"
else
  source "${controlfolder}/libgl_default.txt"
fi

# Setup internet multiplayer game discovery
# Default settings
#export NOX_NO_INTERNET_SERVERS=0 # 1 switches off internet access but keeps LAN access, 0 allows internet access
#
#export NOX_LOBBY_HOST=nox.nwca.xyz
#export NOX_LOBBY_PORT=8088
#export NOX_LOBBY_PATH="/api/v0/games/list"
#
#export NOX_SERVER_CACHE_TTL=30 # How long to cache internet game queries - minimum 30 seconds
#
## If there are bad servers that crash the game they can be filtered using this list
#export NOX_BAD_SERVER_IPS="127.1.1.1,127.1.1.2"
export NOX_BAD_SERVER_NAMES="Kor) Newbies,Kor] Newbies"

# NOX_LIMIT_RANGE_ON_RUN - useful for gamepads and steam deck
# limits the range of the mouse when running but only if starting close to center or passing through center
export NOX_LIMIT_RANGE_ON_RUN_GAMEPAD=1
export NOX_LIMIT_RANGE_ON_RUN_MOUSE=0
# export NOX_LIMIT_RANGE_ON_RUN_RADIUS=110 # default is 110 - the radius of the circle

# Disable built-in gamepad support to use gptokeyb2 (might change in future but known to be stable)
export NOX_GAMEPAD=1
export NOX_GAMEPAD_EXIT=1
export NOX_GAMEPAD_INI="$GAMEDIR/$GPTK_CFG"
export NOX_HIDE_MAIN_RES_UI=1

# ------------------------------------------------------------
# Resolution selection rules:
#
# 1) Default is 640x480x16
#
# 2) Aspect-ratio based behavior:
#    - 4:3 displays:
#        Use the display resolution directly if < 1024x768,
#        otherwise clamp to 1024x768.
#
#    - 1:1 displays (square):
#        Clamp to a square resolution, max 768x768.
#
#    - Widescreen displays (16:9, 16:10, etc.):
#        Use width up to 1024, and compute height to preserve
#        the display's aspect ratio.
#
# 3) Absolute limits:
#    - Width  <= 1024
#    - Height <= 768
# ------------------------------------------------------------

# Hard-coded defaults
NOX_GAME_WIDTH=640
NOX_GAME_HEIGHT=480
NOX_GAME_BITS=16
NOX_GAME_FULLSCREEN=1

if [ -n "$DISPLAY_WIDTH" ] && [ -n "$DISPLAY_HEIGHT" ]; then
    case "$DISPLAY_WIDTH$DISPLAY_HEIGHT" in
        (*[!0-9]*)
            # Non-numeric input â keep defaults
            ;;
        (*)
            # Turn on linear scaling if display is larger than 1024x768
            if [ "$DISPLAY_WIDTH" -gt 1024 ] || [ "$DISPLAY_HEIGHT" -gt 768 ]; then
                export NOX_LINEAR_SCALING=1
            fi

            # Calculate aspect ratio as a float
            ASPECT=$(awk "BEGIN { printf \"%.4f\", $DISPLAY_WIDTH / $DISPLAY_HEIGHT }")

            # 4:3 â 1.3333
            if awk "BEGIN { exit !($ASPECT > 1.30 && $ASPECT < 1.36) }"; then
                if [ "$DISPLAY_WIDTH" -lt 1024 ] && [ "$DISPLAY_HEIGHT" -lt 768 ]; then
                    NOX_GAME_WIDTH="$DISPLAY_WIDTH"
                    NOX_GAME_HEIGHT="$DISPLAY_HEIGHT"
                else
                    NOX_GAME_WIDTH=1024
                    NOX_GAME_HEIGHT=768
                fi

            # 1:1 â 1.0
            elif awk "BEGIN { exit !($ASPECT > 0.98 && $ASPECT < 1.02) }"; then
                # Square resolution, capped
                if [ "$DISPLAY_WIDTH" -lt 768 ]; then
                    NOX_GAME_WIDTH="$DISPLAY_WIDTH"
                    NOX_GAME_HEIGHT="$DISPLAY_WIDTH"
                else
                    NOX_GAME_WIDTH=768
                    NOX_GAME_HEIGHT=768
                fi

            # Widescreen (everything else)
            else
                # Cap width at 1024
                if [ "$DISPLAY_WIDTH" -gt 1024 ]; then
                    NOX_GAME_WIDTH=1024
                else
                    NOX_GAME_WIDTH="$DISPLAY_WIDTH"
                fi

                # Scale height to preserve aspect ratio
                NOX_GAME_HEIGHT=$(awk "BEGIN {
                    h = $NOX_GAME_WIDTH / $ASPECT;
                    if (h > 768) h = 768;
                    printf \"%d\", h
                }")
            fi
            ;;
    esac
fi

# force scaling options
#export NOX_LINEAR_SCALING=1
#export NOX_INTEGER_SCALING=0

# Ensure asset directory exists
mkdir -p "$ASSET_DIR"

# If config does not exist in assets, copy it from game dir
if [ ! -f "$ASSET_DIR/nox.cfg" ]; then
  if [ -n "$DISPLAY_WIDTH" ] && [ "$DISPLAY_WIDTH" -gt 1024 ] && [ -f "$GAMEDIR/nox.cfg.hires" ]; then
    cp "$GAMEDIR/nox.cfg.hires" "$ASSET_DIR/nox.cfg"
    echo "Copied $GAMEDIR/nox.cfg.hires to $ASSET_DIR/nox.cfg"
  elif [ -f "$GAMEDIR/nox.cfg.default" ]; then
    cp "$GAMEDIR/nox.cfg.default" "$ASSET_DIR/nox.cfg"
    echo "Copied $GAMEDIR/nox.cfg.default to $ASSET_DIR/nox.cfg"
  else
    echo "ERROR: Source config not found at $GAMEDIR/nox.cfg.default or $GAMEDIR/nox.cfg.hires" >&2
    exit 1
  fi
fi

if [ -f "$ASSET_DIR/nox.cfg" ]; then
  # Update VideoMode line in config
  sed -i -E \
    "s/^VideoMode.*/VideoMode = ${NOX_GAME_WIDTH} ${NOX_GAME_HEIGHT} ${NOX_GAME_BITS}/" \
    "$ASSET_DIR/nox.cfg"

  sed -i -E \
    "s/^Fullscreen.*/Fullscreen = ${NOX_GAME_FULLSCREEN}/" \
    "$ASSET_DIR/nox.cfg"
fi

export LD_LIBRARY_PATH="/usr/lib32:$LD_LIBRARY_PATH"
if [ "$LIBGL_FB" != "" ]; then
    export LD_LIBRARY_PATH="$GAMEDIR/gl4es.${RUN_ARCH}:$LD_LIBRARY_PATH"
fi

export LD_LIBRARY_PATH="$GAMEDIR/openal.${RUN_ARCH}:$LD_LIBRARY_PATH"
export LD_LIBRARY_PATH="$GAMEDIR/ffmpeg.${RUN_ARCH}:$LD_LIBRARY_PATH"

# Help debug OpenAL issues
#export ALSOFT_LOGLEVEL=3

#$GPTOKEYB2 "$BINARY" -c "$GAMEDIR/$GPTK_CFG" >/dev/null &

#Custom build of gptokeyb2
#$ESUDO env LD_PRELOAD=$controlfolder/libinterpose.${DEVICE_ARCH}.so $GAMEDIR/gptokeyb2.${DEVICE_ARCH} $ESUDOKILL2 "$BINARY" -c "$GAMEDIR/$GPTK_CFG" >/dev/null &
# Line above or below to enable consistent logging
#export LD_PRELOAD=$controlfolder/libinterpose.${DEVICE_ARCH}.so
#"$GAMEDIR/gptokeyb2.${DEVICE_ARCH}" -1 "$BINARY" -c "$GAMEDIR/$GPTK_CFG" > "$GAMEDIR/gptk.txt" 2>&1 &
#unset LD_PRELOAD

# for copying to shell - todo: delete
#export LD_PRELOAD=/mnt/mmc/MUOS/PortMaster/libinterpose.aarch64.so
#"/mnt/sdcard/ports/nox-decomp/gptokeyb2" -1 noxd -c "/mnt/sdcard/ports/nox-decomp/nox.gptk" > "/mnt/sdcard/ports/nox-decomp/gptk.txt" 2>&1 &
#unset LD_PRELOAD

pm_platform_helper "$BINARY"
export LD_PRELOAD="$GAMEDIR/openal.${RUN_ARCH}/libopenal.so.1"

if [[ "${CFW_NAME}" == "Batocera" && "${DEVICE_HAS_X86}" == "Y" ]]; then
  cd "$(dirname "$(readlink -f "$0")")"

  unclutter-remote -h
  export LD_LIBRARY_PATH=/lib32
  export LIBGL_DRIVERS_PATH=/lib32/dri
  export SPA_PLUGIN_DIR="/lib32/spa-0.2:/usr/lib/spa-0.2"
  export PIPEWIRE_MODULE_DIR="/lib32/pipewire-0.3:/usr/lib/pipewire-0.3"
fi

export # for debugging
"$GAMEDIR/$BINARY.${RUN_ARCH}"
unset LD_PRELOAD

pm_finish
