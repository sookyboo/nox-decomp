#!/bin/bash
SELF_SH="$(readlink -f -- "${BASH_SOURCE[0]}" 2>/dev/null || realpath -- "${BASH_SOURCE[0]}")"
SELF_DIR="$(cd -- "$(dirname -- "${SELF_SH}")" && pwd)"

DEVICE_ARCH="aarch64"
DEVICE_HAS_ARMHF="N"
DEVICE_HAS_AARCH64="N"
DEVICE_HAS_X86="N"
DEVICE_HAS_X86_64="N"

if [ -f "/lib/ld-linux-armhf.so.3" ]; then
  DEVICE_ARCH="armhf"
  DEVICE_HAS_ARMHF="Y"
fi

if [ -f "/lib/ld-linux-aarch64.so.1" ]; then
  DEVICE_ARCH="aarch64"
  DEVICE_HAS_AARCH64="Y"
fi

if [ -e "/lib/ld-linux.so.2" ] || [ -e "/usr/lib/ld-linux.so.2" ] || [ "$(uname -i 2>/dev/null || true)" = "i686" ]; then
  DEVICE_ARCH="x86"
  DEVICE_HAS_X86="Y"
fi

if [ -e "/lib/ld-linux-x86-64.so.2" ] || [ -e "/usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2" ] || [ "$(uname -i 2>/dev/null || true)" = "x86_64" ]; then
  DEVICE_ARCH="x86_64"
  DEVICE_HAS_X86_64="Y"
fi

# 1) Default: run arch follows device arch
RUN_ARCH="${DEVICE_ARCH}"

# 2) Remap device arch -> desired run arch
case "${RUN_ARCH}" in
  aarch64|arm64|armv8*)
    RUN_ARCH="armhf"
    ;;
  armhf|armv7*|arm)
    RUN_ARCH="armhf"
    ;;
  amd64|x86_64)
    RUN_ARCH="i386"
    ;;
  x86|i686|i386)
    RUN_ARCH="i386"
    ;;
esac

# ---------------------------
# Paths
# ---------------------------
GAMEDIR="$PWD"
CONF_DIR="$GAMEDIR/conf"
ASSET_DIR="$GAMEDIR/gamefiles/app"
SAVE_DIR="$ASSET_DIR/Save"
GPTK_CFG="nox.gptk2.ini"
GPTK_CFG_FULL="$GAMEDIR/nox.gptk2.ini"
BINARY="noxd"
UTILDIR="$GAMEDIR/utils"

INNOEXTRACT="$GAMEDIR/utils/innoextract.$DEVICE_ARCH"
DATADIR="$GAMEDIR/data"
INSTALLER_EXE_GLOB="setup_nox*.exe"
STEAM_SHORTCUT_MARKER="$ASSET_DIR/steam_shortcut.txt"
# ---------------------------
# Logging
# ---------------------------
mkdir -p "$GAMEDIR"
> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

# ---------------------------
# Optional GUI helpers (zenity) + optional Steam integration (python3)
# ---------------------------
have_zenity=0
have_python=0
have_display=0
have_gui=0

command -v zenity >/dev/null 2>&1 && have_zenity=1
command -v python3 >/dev/null 2>&1 && have_python=1
[[ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]] && have_display=1
[[ "$have_zenity" == "1" && "$have_display" == "1" ]] && have_gui=1

zinfo() {
  # Args: text
  [[ "$have_gui" == "1" ]] || return 0
  zenity --info --title="Nox-Decomp" --width=520 --text="$1" >/dev/null 2>&1 || true
}

zerror() {
  # Args: text
  [[ "$have_gui" == "1" ]] || return 0
  zenity --error --title="Nox-Decomp" --width=520 --text="$1" >/dev/null 2>&1 || true
}

zquestion() {
  # Args: text
  [[ "$have_gui" == "1" ]] || return 1
  zenity --question --title="Nox-Decomp" --width=520 --text="$1" >/dev/null 2>&1
}

zenity_pulse_start() {
  # Args: title text status_file(optional)
  [[ "$have_gui" == "1" ]] || return 1
  local title="$1"
  local text="$2"
  local status_file="${3:-}"

  (
    while :; do
      echo "10"
      if [[ -n "$status_file" && -f "$status_file" ]]; then
        echo "# $(cat "$status_file" 2>/dev/null || echo "$text")"
      else
        echo "# $text"
      fi
      sleep 0.8
    done
  ) | zenity --progress \
        --title="$title" \
        --text="$text" \
        --pulsate \
        --no-cancel \
        --auto-close \
        --width=520 \
        >/dev/null 2>&1 &
  echo $!
}

zenity_pulse_stop() {
  # Args: pid
  local pid="${1:-}"
  [[ -n "$pid" ]] || return 0
  kill "$pid" >/dev/null 2>&1 || true
  wait "$pid" >/dev/null 2>&1 || true
}

maybe_prompt_add_to_steam() {
  # Optional: only runs if zenity + python3 + helper script exists.
  local want_skip=0

  # 0) Skip if user previously said "no"
  if [[ -f "${STEAM_SHORTCUT_MARKER:-$ASSET_DIR/steam_shortcut.txt}" ]]; then
    want_skip=1
  fi

  # 1) Skip if launched via Steam
  if [[ -n "${SteamAppId:-}" || -n "${SteamGameId:-}" ]]; then
    want_skip=1
  fi

  # 2) Skip if user passed our explicit flag
  for a in "$@"; do
    if [[ "$a" == "--skip-steam-install" ]]; then
      want_skip=1
      break
    fi
  done

  # 3) Skip if explicitly disabled
  if [[ "${NOX_SKIP_STEAM_INSTALL:-0}" != "0" ]]; then
    want_skip=1
  fi

  # Layout for non-flatpak build:
  #   utils/steam_shortcut.py
  #   gamefiles/app/steamv.png + steamh.png + app icon (optional)
  local STEAM_SHORTCUT_PY="$UTILDIR/steam_shortcut.py"
  local STEAM_V_IMG="$GAMEDIR/steam/steamv.png"
  local STEAM_H_IMG="$GAMEDIR/steam/steamh.png"
  local STEAM_ICON_IMG="$GAMEDIR/steam/io.github.sookyboo.nox-decomp.png"

  if [[ "$want_skip" == "0" && "$have_gui" == "1" && "$have_python" == "1" && -f "$STEAM_SHORTCUT_PY" ]]; then
    # If already installed, skip asking (helper supports this check)
    if python3 "$STEAM_SHORTCUT_PY" --is-installed --name "Nox-Decomp" --exe "$SELF_SH" >/dev/null 2>&1; then
      :
    else
      chosen_steamid="$(python3 "$STEAM_SHORTCUT_PY" --print-detected-steamid 2>/dev/null || true)"

      msg="Add Nox-Decomp to Steam?

Steam user detected

This will create/update a Steam shortcut and install artwork/controller template.

If you choose No, you won't be asked again (a marker will be created).
"

      if zquestion "$msg"; then
        sid_args=()
        if [[ -n "$chosen_steamid" ]]; then
          sid_args=(--steamid "$chosen_steamid")
        fi

        python3 "$STEAM_SHORTCUT_PY" \
          "${sid_args[@]}" \
          --name "Nox-Decomp" \
          --exe "$SELF_SH" \
          --startdir "$GAMEDIR" \
          --launch="--skip-steam-install" \
          --grid "$STEAM_V_IMG" \
          --portrait "$STEAM_V_IMG" \
          --hero "$STEAM_H_IMG" \
          --icon-file "$STEAM_ICON_IMG" \
          --template "controller_neptune_gamepad+mouse.vdf" \
          --force-template \
          2>&1 || true

        zinfo "Steam shortcut install attempted.

Restart Steam to see changes."
      else
        # User said "No": write marker so we don't ask again
        mkdir -p "$ASSET_DIR" >/dev/null 2>&1 || true
        {
          echo "User declined Steam shortcut prompt on $(date)"
          echo "Delete this file to re-enable prompting."
        } > "${STEAM_SHORTCUT_MARKER:-$ASSET_DIR/steam_shortcut.txt}" 2>/dev/null || true
      fi
    fi
  fi
}

# Run the optional Steam prompt early (no-op unless prerequisites exist)
maybe_prompt_add_to_steam "$@"

# ---------------------------
# Create config/save dir
# ---------------------------
$ESUDO mkdir -p "$CONF_DIR/Save"
$ESUDO mkdir -p "$SAVE_DIR"
bind_directories "$SAVE_DIR" "$CONF_DIR/Save"

if [ ! -d "$ASSET_DIR" ]; then
  echo "Nox game assets not found in gamefiles/"
  # exit 1
fi



install() {
  # -------------------------------------------------
  # Locate source data
  # -------------------------------------------------
  local SRC="$GAMEDIR/gamefiles"
  local NEEDED="$SRC/app/gamedata.bin"

  # -------------------------------------------------
  # Nothing to do if already extracted
  # -------------------------------------------------
  if [[ -f "$NEEDED" ]]; then
    return 0
  fi

  echo "Nox data not extracted"
  sleep 1

  if [[ -z "$SRC" ]]; then
    echo "Put Nox files in:"
    echo "$GAMEDIR/gamefiles/"
    sleep 5
    exit 1
  fi

  mkdir -p "$DATADIR"

  # -------------------------------------------------
  # Find installer
  # -------------------------------------------------
  local found_installer="no"
  local file=""
  shopt -s nullglob nocaseglob
  for file in "$SRC"/$INSTALLER_EXE_GLOB; do
    if [[ -f "$file" ]]; then
      found_installer="yes"
      break
    fi
  done
  shopt -u nocaseglob

  # -------------------------------------------------
  # GUI fallback: pick installer if not present
  # -------------------------------------------------
  if [[ "$found_installer" != "yes" && "${have_gui}" == "1" ]]; then
    local msg
    msg="Nox game data not found.

Please select your Nox GOG installer (setup_nox*.exe).

The installer will be copied into:
  $SRC/
and extracted automatically."
    zinfo "$msg"

    local picked_exe=""
    picked_exe="$(zenity --file-selection \
      --title="Select Nox GOG installer" \
      --file-filter="Windows installer (*.exe) | *.exe" \
      --file-filter="All files | *" \
      2>/dev/null || true
    )"

    if [[ -n "$picked_exe" && -f "$picked_exe" ]]; then
      local bn
      bn="$(basename "$picked_exe")"
      echo "User selected installer via zenity: $picked_exe"
      mkdir -p "$SRC"
      cp -f "$picked_exe" "$SRC/$bn" >/dev/null 2>&1 || true
    fi

    # re-scan after copy
    found_installer="no"
    shopt -s nullglob nocaseglob
    for file in "$SRC"/$INSTALLER_EXE_GLOB; do
      if [[ -f "$file" ]]; then
        found_installer="yes"
        break
      fi
    done
    shopt -u nocaseglob
  fi

  # -------------------------------------------------
  # If we have an installer, extract it with zenity updates (audio-conversion style)
  # -------------------------------------------------
  if [[ "$found_installer" == "yes" ]]; then
    echo "Found Nox GOG installer"
    echo "Extracting GOG installer"
    sleep 1

    # Prefer a concrete installer path (first match) for nicer status + no glob surprises
    local installer_path=""
    shopt -s nullglob nocaseglob
    for file in "$SRC"/$INSTALLER_EXE_GLOB; do
      if [[ -f "$file" ]]; then
        installer_path="$file"
        break
      fi
    done
    shopt -u nocaseglob

    if [[ -z "$installer_path" ]]; then
      echo "ERROR: installer glob matched earlier but no file found on second pass"
      zerror "Extraction failed.

Could not locate installer after selection.

Please check log.txt for details."
      sleep 5
      exit 1
    fi

    # --- start pulsing dialog (same style as convert_dialog) ---
    local zpid=""
    local zstatus=""
    if [[ "$have_gui" == "1" ]]; then
      zstatus="$(mktemp)"
      printf 'Extracting game data…\nStarting innoextract…\n' >"$zstatus"

      (
        while :; do
          echo "10"
          echo "# $(cat "$zstatus" 2>/dev/null || echo "Extracting game data…")"
          sleep 0.8
        done
      ) | zenity --progress \
            --title="Nox-Decomp" \
            --text="Extracting game data…" \
            --pulsate \
            --no-cancel \
            --auto-close \
            --width=520 \
            >/dev/null 2>&1 &
      zpid=$!
    fi

    # --- run innoextract and update status file directly (no tail/tee buffering) ---
    local rc=0
    if [[ -n "$zstatus" ]]; then
      local -a exec_cmd
      if command -v stdbuf >/dev/null 2>&1; then
        exec_cmd=(stdbuf -oL -eL "$INNOEXTRACT" "$installer_path" -d "$SRC")
      else
        exec_cmd=("$INNOEXTRACT" "$installer_path" -d "$SRC")
      fi

      "${exec_cmd[@]}" 2>&1 | while IFS= read -r line; do
        # Preserve full output into log.txt (pipe would otherwise bypass your global tee)
        echo "$line"
        line="${line//$'\r'/}"
        [[ -n "$line" ]] && printf 'Extracting…\n%s\n' "$line" >"$zstatus" 2>/dev/null || true
      done
      rc=${PIPESTATUS[0]}
    else
      "$INNOEXTRACT" "$installer_path" -d "$SRC"
      rc=$?
    fi

    # --- stop zenity cleanly ---
    if [[ -n "$zpid" ]]; then
      kill "$zpid" >/dev/null 2>&1 || true
      wait "$zpid" >/dev/null 2>&1 || true
    fi
    [[ -n "$zstatus" ]] && rm -f "$zstatus" >/dev/null 2>&1 || true

    if [[ "$rc" -ne 0 ]]; then
      echo "Extraction failed (rc=$rc)"
      zerror "Extraction failed.

Please check log.txt for details."
      sleep 5
      exit 1
    fi
  fi

  # -------------------------------------------------
  # Verify extracted output
  # -------------------------------------------------
  echo "Extracting Nox data..."
  echo "Extraction may take up to 10-60min"
  # Produces gamefiles/app/gamedata.bin if successful

  if [[ ! -f "$NEEDED" ]]; then
    echo "Extraction failed"
    zerror "Extraction failed.

Expected:
  $NEEDED

Put Nox files in:
  $GAMEDIR/gamefiles/"
    sleep 5
    exit 1
  fi

  echo "Extraction complete"
  sleep 1

  echo "Delete installer files."
  #rm -fR "$SRC"/$INSTALLER_EXE_GLOB
  sleep 1
}

convert_dialog() {
  NEEDED="$ASSET_DIR/gamedata.bin"
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
    echo "32-bit system detected, skipping dialog audio conversion"
    return 0
  fi

  # -------------------------------------------------
  # Only run if game data exists
  # -------------------------------------------------
  if [ ! -f "$NEEDED" ]; then
    echo "Game data not present, skipping dialog conversion"
    return 0
  fi



  # -------------------------------------------------
  # Preconditions
  # -------------------------------------------------
  if [ ! -x "$FFMPEG_BIN" ]; then
    echo "ERROR: ffmpeg not found at $FFMPEG_BIN"
    return 1
  fi

  if [ ! -d "$DIALOG_DIR" ]; then
    echo "ERROR: Dialog directory not found"
    return 1
  fi

  # -------------------------------------------------
  # Gather WAV files
  # -------------------------------------------------
  shopt -s nullglob nocaseglob
  wav_files=("$DIALOG_DIR"/*.wav)
  total="${#wav_files[@]}"
  shopt -u nocaseglob nullglob

  if [ "$total" -eq 0 ]; then
    echo "No dialog WAV files found, skipping conversion"
    return 0
  fi

  echo "Converting dialog audio ($total files)"
  sleep 1

  zpid=""
  zstatus=""
  if [[ "$have_gui" == "1" ]]; then
    zstatus="$(mktemp)"
    printf 'Converting dialog audio… (0/%s)\n' "$total" >"$zstatus"
    zpid="$(zenity_pulse_start "Nox-Decomp" "Converting dialog audio…" "$zstatus")" || true
  fi

  # -------------------------------------------------
  # Convert with progress updates
  # -------------------------------------------------
  i=0
  for wav in "${wav_files[@]}"; do
    i=$((i + 1))
    base="$(basename "$wav")"
    if [[ -n "$zstatus" ]]; then
      printf 'Converting audio (%s/%s): %s\n' "$i" "$total" "$base" >"$zstatus" 2>/dev/null || true
    fi
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
      if [[ -n "$zpid" ]]; then
        zenity_pulse_stop "$zpid"
      fi
      [[ -n "$zstatus" ]] && rm -f "$zstatus" >/dev/null 2>&1 || true
      zerror "Dialog audio conversion failed on:
  $base

See log.txt for details."
      return 1
    fi

  done

  if [[ -n "$zpid" ]]; then
    zenity_pulse_stop "$zpid"
  fi
  [[ -n "$zstatus" ]] && rm -f "$zstatus" >/dev/null 2>&1 || true

  # -------------------------------------------------
  # Finish up
  # -------------------------------------------------
  echo "Dialog audio converted to PCM on $(date)" > "$MARKER_FILE"

  echo "Dialog audio conversion complete"
  sleep 1
  zinfo "Dialog audio conversion complete."
}

# -------------------------------------------------
# Install game data
# -------------------------------------------------
install
convert_dialog

# ---------------------------
# Runtime environment
# ---------------------------
cd "$ASSET_DIR"

export XDG_DATA_HOME="$CONF_DIR"

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
NOX_GAME_WIDTH=1024
NOX_GAME_HEIGHT=768
NOX_GAME_BITS=16
NOX_GAME_FULLSCREEN=1

if [ -n "$DISPLAY_WIDTH" ] && [ -n "$DISPLAY_HEIGHT" ]; then
    case "$DISPLAY_WIDTH$DISPLAY_HEIGHT" in
        (*[!0-9]*)
            # Non-numeric input â keep defaults
            ;;
        (*)
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

# Ensure asset directory exists
mkdir -p "$ASSET_DIR"

# If config does not exist in assets, copy it from game dir
if [ ! -f "$ASSET_DIR/nox.cfg" ]; then
  if [ -f "$GAMEDIR/nox.cfg" ]; then
    cp "$GAMEDIR/nox.cfg" "$ASSET_DIR/nox.cfg"
  else
    echo "ERROR: Source config not found at $GAMEDIR/nox.cfg" >&2
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

#export LD_LIBRARY_PATH="/usr/lib32:$LD_LIBRARY_PATH"
#if [ "$LIBGL_FB" != "" ]; then
#    export LD_LIBRARY_PATH="$GAMEDIR/gl4es.${RUN_ARCH}:$LD_LIBRARY_PATH"
#fi
#
#export LD_LIBRARY_PATH="$GAMEDIR/openal.${RUN_ARCH}:$LD_LIBRARY_PATH"
export LD_LIBRARY_PATH="$GAMEDIR/ffmpeg.${RUN_ARCH}:$LD_LIBRARY_PATH"

# Help debug OpenAL issues
#export ALSOFT_LOGLEVEL=3

export LD_LIBRARY_PATH="$GAMEDIR/steam/libs.i386.x11:$LD_LIBRARY_PATH" # Load libSDL2 with x11/wayland support
export # for debugging

export NOX_CONTROL_SERVER=0
export NOX_CONTROL_SERVER_PASSWORD=secret
# optional
export NOX_CONTROL_SERVER_BIND=127.0.0.1
export NOX_CONTROL_SERVER_PORT=2323
export NOX_CAPTURE_INPUT=0   # optional
export NOX_NET_LOG=0
export NOX_WSA_LOG=0
export NOX_PACKET_LOG=0
#export NOX_CONTROL_SERVER_BOOT="macro server; gamekill;"

export NOX_SKIP_INTRO_MOVIES=0

export NOX_LOBBY_REGISTER_ENABLE=0
export NOX_UPNP_ENABLE=0

export NOX_LIMIT_RANGE_ON_RUN=1
export NOX_LIMIT_RANGE_ON_RUN_RADIUS=118
: "${NOX_GAMEPAD_INI:=${GPTK_CFG_FULL}}"
export NOX_GAMEPAD_INI

: "${NOX_GAMEPAD_EXIT:=1}"
export NOX_GAMEPAD_EXIT

: "${NOX_GAMEPAD:=1}"
export NOX_GAMEPAD

"$GAMEDIR/$BINARY.${RUN_ARCH}"
unset LD_PRELOAD
