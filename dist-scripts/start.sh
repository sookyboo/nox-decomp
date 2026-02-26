#!/usr/bin/env bash
set -euo pipefail

APP_ID="io.github.sookyboo.nox-decomp"

PKG_BASE="/app/share/nox-decomp"
PKG_NOXD="/app/bin/noxd.i386"
PKG_GPTK2_INI="${PKG_BASE}/nox.gptk2.ini"
PKG_INNOEXTRACT="/app/bin/innoextract"
PKG_FFMPEG_X64="/usr/bin/ffmpeg"
PKG_NOX_CFG="${PKG_BASE}/nox.cfg"

# ---------------------------
# Optional Steam integration prompt (zenity + python3)
# ---------------------------

STEAM_SHORTCUT_PY="/app/bin/steam_shortcut.py"
STEAM_V_IMG="${PKG_BASE}/steamv.png"
STEAM_H_IMG="${PKG_BASE}/steamh.png"
STEAM_ICON_IMG="${PKG_BASE}/${APP_ID}.png"  # staged icon in your flatpak
want_skip=0

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

have_zenity=0
have_python=0
command -v zenity >/dev/null 2>&1 && have_zenity=1
command -v python3 >/dev/null 2>&1 && have_python=1

# Only try GUI prompt if we have a display
have_display=0
[[ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]] && have_display=1

if [[ "$want_skip" == "0" && "$have_display" == "1" && "$have_zenity" == "1" && "$have_python" == "1" && -f "$STEAM_SHORTCUT_PY" ]]; then
  # If already installed, skip asking
  if python3 "$STEAM_SHORTCUT_PY" --is-installed-flatpak "${APP_ID}" >/dev/null 2>&1; then
    :
  else
    chosen_steamid="$(python3 "$STEAM_SHORTCUT_PY" --print-detected-steamid 2>/dev/null || true)"

    msg="Add Nox-Decomp to Steam?

Steam user detected: ${chosen_steamid:-unknown}

This will create/update a Steam shortcut, install artwork, and set a Steam Deck controller template.

You can skip this prompt in future by installing the steam shortcut or launching with:
  --skip-steam-install"

    if zenity --question --title="Nox-Decomp" --width=520 --text="$msg"; then
      sid_args=()
      if [[ -n "$chosen_steamid" ]]; then
        sid_args=(--steamid "$chosen_steamid")
      fi

      python3 "$STEAM_SHORTCUT_PY" \
        "${sid_args[@]}" \
        --name "Nox-Decomp" \
        --exe "/usr/bin/flatpak" \
        --startdir "$HOME" \
        --launch "run ${APP_ID} --skip-steam-install" \
        --flatpak-app-id "${APP_ID}" \
        --grid "$STEAM_V_IMG" \
        --portrait "$STEAM_V_IMG" \
        --hero "$STEAM_H_IMG" \
        --icon-file "$STEAM_ICON_IMG" \
        --template "controller_neptune_gamepad+mouse.vdf" \
        --force-template \
        --sspy-parser-id 1 \
        --tags "Installed,Ready TO Play" \
        >/dev/null 2>&1 || true

      zenity --info --title="Nox-Decomp" --width=520 --text="Steam shortcut install attempted.

Restart Steam to see changes."
    fi
  fi
fi

: "${NOX_GAMEPAD_INI:=${PKG_GPTK2_INI}}"
export NOX_GAMEPAD_INI

: "${NOX_GAMEPAD_EXIT:=1}"
export NOX_GAMEPAD_EXIT


# Host-writable dirs
: "${NOX_HOME_DIR:=${HOME}/nox-decomp}"
: "${NOX_ASSET_DIR:=${NOX_HOME_DIR}/gamefiles/app}"
: "${NOX_CONF_DIR:=${NOX_HOME_DIR}/conf}"
: "${NOX_SAVE_DIR:=${NOX_ASSET_DIR}/Save}"
mkdir -p "${NOX_ASSET_DIR}" "${NOX_CONF_DIR}" "${NOX_SAVE_DIR}"
cd "${NOX_ASSET_DIR}"
export XDG_DATA_HOME="${NOX_CONF_DIR}"

> "$NOX_ASSET_DIR/log.txt" && exec > >(tee "$NOX_ASSET_DIR/log.txt") 2>&1

# Sanity
if [[ ! -f "${PKG_NOXD}" ]]; then
  echo "ERROR: missing ${PKG_NOXD}" >&2
  echo "Installed /app tree:" >&2
  find /app -maxdepth 4 -print >&2 || true
  exit 127
fi

# ---------------------------
# Zenity helpers (only if display + zenity)
# ---------------------------
have_gui=0
if command -v zenity >/dev/null 2>&1 && [[ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]]; then
  have_gui=1
fi

zenity_pulse() {
  # Args: title text
  local title="$1"
  local text="$2"
  if [[ "$have_gui" != "1" ]]; then
    return 1
  fi

  # Start a pulsing progress dialog and print its PID.
  (
    echo "# $text"
    while :; do
      echo "10"
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

# ---------------------------
# Game data extraction + dialog conversion (one-time)
# ---------------------------
: "${NOX_SKIP_EXTRACT:=0}"
: "${NOX_SKIP_DIALOG_CONVERT:=0}"
: "${NOX_FORCE_EXTRACT:=0}"
: "${NOX_FORCE_DIALOG_CONVERT:=0}"

NOX_GAMEFILES_DIR="${NOX_HOME_DIR}/gamefiles"
NOX_GAME_DATA_BIN="${NOX_GAMEFILES_DIR}/app/gamedata.bin"
NOX_INSTALLER_GLOB="${NOX_GAMEFILES_DIR}/setup_nox"*.exe

install_game_data() {
  # Skip unless missing (or forced)
  if [[ "${NOX_FORCE_EXTRACT}" == "0" && -f "${NOX_GAME_DATA_BIN}" ]]; then
    return 0
  fi

  if [[ ! -x "${PKG_INNOEXTRACT}" ]]; then
    echo "WARN: innoextract not found/executable at ${PKG_INNOEXTRACT}; cannot extract installer." >&2
    echo "Put extracted game files in: ${NOX_GAMEFILES_DIR}/app/ (need gamedata.bin)" >&2
    return 0
  fi

  mkdir -p "${NOX_GAMEFILES_DIR}"

  shopt -s nullglob nocaseglob
  installers=( ${NOX_INSTALLER_GLOB} )
  shopt -u nocaseglob

  if (( ${#installers[@]} == 0 )); then
    msg=$'Nox data not extracted.\n\n'"Place GOG installer matching 'setup_nox*.exe' in:\n${NOX_GAMEFILES_DIR}/\n\nOr manually provide extracted files so this exists:\n${NOX_GAME_DATA_BIN}"

    echo "Nox data not extracted." >&2
    echo "Place GOG installer matching 'setup_nox*.exe' in: ${NOX_GAMEFILES_DIR}/" >&2
    echo "Or manually provide extracted files so this exists: ${NOX_GAME_DATA_BIN}" >&2

    # Best-effort GUI prompt (only if zenity + a display is available)
    if command -v zenity >/dev/null 2>&1 && [[ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]]; then
      zenity --info --title="Nox-Decomp" --width=520 --text="$msg" >/dev/null 2>&1 || true
    fi

    exit 0
  fi

  echo "Found Nox GOG installer: ${installers[0]}"
  echo "Extracting installer into: ${NOX_GAMEFILES_DIR}"
  echo "Extraction may take a couple of minutes"

  local zpid=""
  local zstatus=""
  local zlog=""

  if [[ "${have_gui:-0}" == "1" ]]; then
    zstatus="$(mktemp)"
    zlog="$(mktemp)"
    printf 'Extracting game data…\nStarting innoextract…\n' >"$zstatus"

    (
      while :; do
        echo "10"
        if [[ -s "$zlog" ]]; then
          last="$(tail -n 1 "$zlog" 2>/dev/null | tr -d '\r')"
          printf '# %s\n' "$(cat "$zstatus" 2>/dev/null)"
          if [[ -n "$last" ]]; then
            printf '# Extracting setup file: %s\n' "$last"
          fi
        else
          printf '# %s\n' "$(cat "$zstatus" 2>/dev/null)"
        fi
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

  # Run innoextract; if GUI is active, capture output to zlog for live tailing
  if [[ -n "${zstatus:-}" ]]; then
    printf 'Running innoextract…\n' >"$zstatus" 2>/dev/null || true
    # Keep stdout+stderr for the zenity tail and for your log.txt
    "${PKG_INNOEXTRACT}" "${installers[0]}" -d "${NOX_GAMEFILES_DIR}" 2>&1 | tee -a "$zlog"
    rc=${PIPESTATUS[0]}
  else
    "${PKG_INNOEXTRACT}" "${installers[0]}" -d "${NOX_GAMEFILES_DIR}"
    rc=$?
  fi

  # Stop zenity cleanly
  if [[ -n "${zpid}" ]]; then
    kill "${zpid}" >/dev/null 2>&1 || true
    wait "${zpid}" 2>/dev/null || true
  fi
  [[ -n "${zstatus:-}" ]] && rm -f "$zstatus" 2>/dev/null || true
  [[ -n "${zlog:-}" ]] && rm -f "$zlog" 2>/dev/null || true

  if [[ "$rc" -ne 0 ]]; then
    echo "ERROR: innoextract failed (rc=$rc)" >&2
    return 1
  fi

  if [[ ! -f "${NOX_GAME_DATA_BIN}" ]]; then
    echo "ERROR: extraction finished but ${NOX_GAME_DATA_BIN} still missing." >&2
    return 1
  fi

  echo "Remove installer nox.cfg"
  rm "$NOX_GAMEFILES_DIR/app/nox.cfg" || true

  echo "Extraction complete"
}

ensure_nox_cfg() {
  local dst_cfg="${NOX_ASSET_DIR}/nox.cfg"
  if [[ -f "${dst_cfg}" ]]; then
    return 0
  fi
  if [[ -f "${PKG_NOX_CFG}" ]]; then
    echo "Installing default nox.cfg -> ${dst_cfg}"
    cp -f "${PKG_NOX_CFG}" "${dst_cfg}"
    return 0
  fi
  echo "WARN: nox.cfg missing and packaged default not found at ${PKG_NOX_CFG}" >&2
  return 0
}

is_steam_deck() {
  local pn=""
  pn="$(cat /sys/devices/virtual/dmi/id/product_name 2>/dev/null || true)"
  case "$pn" in
    Jupiter|Galileo) return 0 ;;
  esac
  return 1
}

convert_dialog() {
  local marker_file="${NOX_ASSET_DIR}/converted_dialog.txt"
  local dialog_dir="${NOX_ASSET_DIR}/Dialog"

  # Skip unless missing marker (or forced)
  if [[ "${NOX_FORCE_DIALOG_CONVERT}" == "0" && -f "${marker_file}" ]]; then
    return 0
  fi

  # Skip on 32-bit systems (ffmpeg is 64-bit only)
  if [[ "$(getconf LONG_BIT)" == "32" ]]; then
    echo "32-bit system detected, skipping dialog audio conversion"
    return 0
  fi

  # Only run if game data exists
  if [[ ! -f "${NOX_GAME_DATA_BIN}" ]]; then
    echo "Game data not present, skipping dialog conversion"
    return 0
  fi

  if [[ ! -x "${PKG_FFMPEG_X64}" ]]; then
    echo "WARN: ffmpeg not found/executable at ${PKG_FFMPEG_X64}; skipping dialog conversion" >&2
    return 0
  fi

  if [[ ! -d "${dialog_dir}" ]]; then
    echo "Dialog directory not found, skipping conversion"
    return 0
  fi

  shopt -s nullglob nocaseglob
  local wav_files=("${dialog_dir}"/*.wav)
  local total="${#wav_files[@]}"
  shopt -u nocaseglob

  if [[ "${total}" -eq 0 ]]; then
    echo "No dialog WAV files found, skipping conversion"
    return 0
  fi

  echo "Converting dialog audio (${total} files)"

  # --- Start pulsing dialog, but keep its text updated via a status file ---
  local zpid=""
  local zstatus=""
  if [[ "${have_gui:-0}" == "1" ]]; then
    zstatus="$(mktemp)"
    printf 'Converting dialog audio… (0/%s)\n' "$total" >"$zstatus"

    (
      # Feed zenity: pulse + current status text
      while :; do
        echo "10"
        echo "# $(cat "$zstatus" 2>/dev/null || echo "Converting dialog audio…")"
        sleep 0.8
      done
    ) | zenity --progress \
          --title="Nox-Decomp" \
          --text="Converting dialog audio…" \
          --pulsate \
          --no-cancel \
          --auto-close \
          --width=520 \
          >/dev/null 2>&1 &
    zpid=$!
  fi

  local i=0
  for wav in "${wav_files[@]}"; do
    i=$((i + 1))
    local base
    base="$(basename "$wav")"

    # (Optional) write a breadcrumb to your log
    echo "Converting (${i}/${total}): ${base}"
    if [[ -n "${zstatus:-}" ]]; then
      printf 'Converting audio (%s/%s): %s\n' "$i" "$total" "$base" >"$zstatus" 2>/dev/null || true
    fi

    local tmp="${wav}.tmp"
    if "${PKG_FFMPEG_X64}" -y \
        -loglevel error \
        -i "${wav}" \
        -ac 1 \
        -ar 22050 \
        -c:a pcm_s16le \
        -f wav \
        "${tmp}"; then
      mv "${tmp}" "${wav}"
    else
      if [[ -n "${zpid}" ]]; then
        kill "${zpid}" >/dev/null 2>&1 || true
        wait "${zpid}" 2>/dev/null || true
      fi
      [[ -n "${zstatus:-}" ]] && rm -f "$zstatus" 2>/dev/null || true
      echo "ERROR converting ${base}" >&2
      return 1
    fi
  done

  if [[ -n "${zpid}" ]]; then
    kill "${zpid}" >/dev/null 2>&1 || true
    wait "${zpid}" 2>/dev/null || true
  fi
  [[ -n "${zstatus:-}" ]] && rm -f "$zstatus" 2>/dev/null || true

  echo "Dialog audio converted to PCM on $(date)" > "${marker_file}"
  echo "Dialog audio conversion complete"

  # If we’re on Steam Deck + have zenity GUI, tell the user to restart into Gaming Mode and exit.
  if [[ "${have_gui:-0}" == "1" ]] && is_steam_deck; then
    zenity --info \
      --title="Nox-Decomp" \
      --width=520 \
      --text=$'Dialog audio conversion finished.\n\nPlease restart the Steam Deck into Gaming Mode (Steam button → Power → Restart).\n\nAfter restarting, launch Nox-Decomp from Steam.' \
      >/dev/null 2>&1 || true
    exit 0
  fi
}

if [[ "${NOX_SKIP_EXTRACT}" == "0" ]]; then
  install_game_data
fi

ensure_nox_cfg

if [[ "${NOX_SKIP_DIALOG_CONVERT}" == "0" ]]; then
  convert_dialog
fi

# Optional: force X11 (helps GLX visual issues on some setups)
: "${NOX_FORCE_X11:=0}"
if [[ "${NOX_FORCE_X11}" != "0" ]]; then
  export SDL_VIDEODRIVER=x11
  # Avoid SDL trying to prefer Wayland when both sockets exist
  export SDL_VIDEO_DRIVER=x11 2>/dev/null || true
fi

detect_display_wh() {
  local w="" h=""
  local method=""

  echo "[display] detect_display_wh: DISPLAY='${DISPLAY:-}' WAYLAND_DISPLAY='${WAYLAND_DISPLAY:-}'" >&2

  # 1) X11: xrandr (best if available)
  if command -v xrandr >/dev/null 2>&1; then
    if [[ -n "${DISPLAY:-}" ]]; then
      echo "[display] trying xrandr (--current)" >&2
      local line res
      line="$(xrandr --current 2>/dev/null | awk '
        $2=="connected" && $3=="primary" {print; exit}
        $2=="connected" {print; exit}
      ')"
      if [[ -n "$line" ]]; then
        echo "[display] xrandr picked line: $line" >&2
        # Example: eDP-1 connected primary 1280x800+0+0 ...
        res="$(awk '{for(i=1;i<=NF;i++) if($i ~ /^[0-9]+x[0-9]+\+/){gsub(/\+.*/,"",$i); print $i; exit}}' <<<"$line")"
        echo "[display] xrandr parsed res: '${res:-}'" >&2
        if [[ "$res" =~ ^([0-9]+)x([0-9]+)$ ]]; then
          w="${BASH_REMATCH[1]}"; h="${BASH_REMATCH[2]}"
          method="xrandr"
          echo "[display] xrandr success: ${w}x${h}" >&2
        else
          echo "[display] xrandr did not yield a usable WxH" >&2
        fi
      else
        echo "[display] xrandr: no connected outputs found" >&2
      fi
    else
      echo "[display] xrandr available but DISPLAY is unset; skipping xrandr" >&2
    fi
  else
    echo "[display] xrandr not found; skipping" >&2
  fi

  # 2) X11: xdpyinfo fallback
  if [[ -z "$w" ]]; then
    if command -v xdpyinfo >/dev/null 2>&1; then
      if [[ -n "${DISPLAY:-}" ]]; then
        echo "[display] trying xdpyinfo" >&2
        local dims
        dims="$(xdpyinfo 2>/dev/null | awk -F'[ x]+' '/dimensions:/{print $3" "$4; exit}')"
        echo "[display] xdpyinfo parsed dims: '${dims:-}'" >&2
        if [[ "$dims" =~ ^([0-9]+)[[:space:]]+([0-9]+)$ ]]; then
          w="${BASH_REMATCH[1]}"; h="${BASH_REMATCH[2]}"
          method="xdpyinfo"
          echo "[display] xdpyinfo success: ${w}x${h}" >&2
        else
          echo "[display] xdpyinfo did not yield a usable WxH" >&2
        fi
      else
        echo "[display] xdpyinfo available but DISPLAY is unset; skipping xdpyinfo" >&2
      fi
    else
      echo "[display] xdpyinfo not found; skipping" >&2
    fi
  fi

  # 3) DRM sysfs (good for docked + wayland/gamescope when tools aren’t there)
  if [[ -z "$w" ]]; then
    echo "[display] trying DRM sysfs: /sys/class/drm/card*-*/status" >&2
    local status_file modes_file mode
    shopt -s nullglob
    local status_files=(/sys/class/drm/card*-*/status)
    shopt -u nullglob

    if (( ${#status_files[@]} == 0 )); then
      echo "[display] DRM sysfs: no status files found" >&2
    else
      echo "[display] DRM sysfs: found ${#status_files[@]} status files" >&2
      for status_file in "${status_files[@]}"; do
        [[ -r "$status_file" ]] || { echo "[display] DRM sysfs: unreadable: $status_file" >&2; continue; }

        local st conn
        st="$(cat "$status_file" 2>/dev/null || true)"
        conn="$(basename "$(dirname "$status_file")")"  # e.g. card0-eDP-1
        echo "[display] DRM sysfs: ${conn} status='${st}'" >&2

        [[ "$st" == "connected" ]] || continue

        modes_file="${status_file%/status}/modes"
        if [[ -r "$modes_file" ]]; then
          mode="$(head -n1 "$modes_file" 2>/dev/null || true)"
          echo "[display] DRM sysfs: ${conn} first mode='${mode}' (from $modes_file)" >&2

          if [[ "$mode" =~ ^([0-9]+)x([0-9]+)$ ]]; then
            w="${BASH_REMATCH[1]}"; h="${BASH_REMATCH[2]}"

            # Steam Deck / internal panels sometimes report 800x1280 (portrait ordering).
            # If it's eDP (internal) and looks portrait, swap to landscape.
            if [[ "$conn" == *"-eDP-"* || "$conn" == *"eDP-"* ]]; then
              if (( w < h )); then
                echo "[display] DRM sysfs: ${conn} looks portrait (${w}x${h}); swapping to ${h}x${w}" >&2
                local tmp="$w"; w="$h"; h="$tmp"
                echo "[display] DRM sysfs: ${conn} after swap: ${w}x${h}" >&2
              fi
            fi

            method="drm:${conn}"
            echo "[display] DRM sysfs success: ${w}x${h} via ${conn}" >&2
            break
          else
            echo "[display] DRM sysfs: ${conn} mode not usable" >&2
          fi
        else
          echo "[display] DRM sysfs: modes file unreadable/missing: $modes_file" >&2
        fi
      done
    fi
  fi

  # 4) Last resort: assume internal Deck panel
  if [[ -z "$w" ]]; then
    w=1280; h=800
    method="fallback:1280x800"
    echo "[display] fallback to ${w}x${h}" >&2
  fi

  echo "[display] detect_display_wh result: ${w}x${h} (method=${method:-unknown})" >&2
  printf '%s %s\n' "$w" "$h"
}

read DISPLAY_WIDTH DISPLAY_HEIGHT < <(detect_display_wh)
export DISPLAY_WIDTH DISPLAY_HEIGHT
echo "[display] detected ${DISPLAY_WIDTH}x${DISPLAY_HEIGHT}"

# ---------------------------
# Auto-pick NOX resolution from DISPLAY_WIDTH / DISPLAY_HEIGHT (if provided)
# ---------------------------

# Optional inputs (may be unset under -u)
dw="${DISPLAY_WIDTH:-}"
dh="${DISPLAY_HEIGHT:-}"

MAX_W=1024
MAX_H=768

read NOX_GAME_WIDTH_DEFAULT NOX_GAME_HEIGHT_DEFAULT < <(
  awk -v dw="$DISPLAY_WIDTH" -v dh="$DISPLAY_HEIGHT" -v mw="$MAX_W" -v mh="$MAX_H" '
    BEGIN {
      sw = mw / dw;
      sh = mh / dh;
      s = sw < sh ? sw : sh;
      if (s > 1) s = 1;          # don’t upscale
      w = int(dw * s);
      h = int(dh * s);
      if (w < 1) w = 1;
      if (h < 1) h = 1;
      printf "%d %d\n", w, h;
    }'
)

# Defaults (only applied if user didn't already export them)
: "${NOX_GAME_WIDTH:=$NOX_GAME_WIDTH_DEFAULT}"
: "${NOX_GAME_HEIGHT:=$NOX_GAME_HEIGHT_DEFAULT}"
: "${NOX_GAME_BITS:=16}"
: "${NOX_GAME_FULLSCREEN:=1}"

export NOX_GAME_WIDTH NOX_GAME_HEIGHT NOX_GAME_BITS NOX_GAME_FULLSCREEN

# (Optional) breadcrumb in log.txt
echo "[video] DISPLAY=${dw:-unset}x${dh:-unset} -> NOX_GAME=${NOX_GAME_WIDTH}x${NOX_GAME_HEIGHT} bits=${NOX_GAME_BITS} fullscreen=${NOX_GAME_FULLSCREEN}"

# ---------------------------
# Apply detected video settings into nox.cfg (best-effort)
# ---------------------------

if [[ -f "${NOX_ASSET_DIR}/nox.cfg" ]]; then
  echo "[cfg] patching ${NOX_ASSET_DIR}/nox.cfg with VideoMode=${NOX_GAME_WIDTH}x${NOX_GAME_HEIGHT}x${NOX_GAME_BITS} Fullscreen=${NOX_GAME_FULLSCREEN}"

  # Update existing keys if present
  sed -i -E \
    "s/^VideoMode[[:space:]]*=.*/VideoMode = ${NOX_GAME_WIDTH} ${NOX_GAME_HEIGHT} ${NOX_GAME_BITS}/" \
    "${NOX_ASSET_DIR}/nox.cfg" || true

  sed -i -E \
    "s/^Fullscreen[[:space:]]*=.*/Fullscreen = ${NOX_GAME_FULLSCREEN}/" \
    "${NOX_ASSET_DIR}/nox.cfg" || true

  # If keys were missing, append them (so it actually takes effect)
  if ! grep -qE '^VideoMode[[:space:]]*=' "${NOX_ASSET_DIR}/nox.cfg" 2>/dev/null; then
    echo "[cfg] VideoMode not found; appending"
    printf '\nVideoMode = %s %s %s\n' "${NOX_GAME_WIDTH}" "${NOX_GAME_HEIGHT}" "${NOX_GAME_BITS}" >> "${NOX_ASSET_DIR}/nox.cfg" || true
  fi
  if ! grep -qE '^Fullscreen[[:space:]]*=' "${NOX_ASSET_DIR}/nox.cfg" 2>/dev/null; then
    echo "[cfg] Fullscreen not found; appending"
    printf 'Fullscreen = %s\n' "${NOX_GAME_FULLSCREEN}" >> "${NOX_ASSET_DIR}/nox.cfg" || true
  fi
else
  echo "[cfg] nox.cfg not found at ${NOX_ASSET_DIR}/nox.cfg; skipping patch"
fi

# ---------------------------
# SDL controller defaults (user can override by exporting their own values)
# ---------------------------
: "${SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD:=1}"
: "${SDL_GAMECONTROLLER_USE_BUTTON_LABELS:=1}"

export SDL_GAMECONTROLLERCONFIG_FILE
export SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD
export SDL_GAMECONTROLLER_IGNORE_DEVICES
export SDL_GAMECONTROLLER_USE_BUTTON_LABELS

# ---------------------------
# Build LIBPATH
# We want GL32 + compat FIRST (if present), then our bundled deps, then steam SDL.
# ---------------------------
add_path_front() {
  local p="$1"
  [[ -d "$p" ]] || return 0
  case ":${LIBPATH:-}:" in
    *":$p:"*) return 0 ;;
  esac
  LIBPATH="${p}${LIBPATH:+:${LIBPATH}}"
}

add_path_back() {
  local p="$1"
  [[ -d "$p" ]] || return 0
  case ":${LIBPATH:-}:" in
    *":$p:"*) return 0 ;;
  esac
  LIBPATH="${LIBPATH:+${LIBPATH}:}${p}"
}

LIBPATH=""

# --- GL32 mount paths (only add if they exist) ---
# Newer runtimes mount GL32 under /app/lib/.../GL/default/lib
add_path_front "/app/lib/i386-linux-gnu/GL/default/lib"
# Some variants may present it under /usr/lib (less common, but harmless)
add_path_front "/usr/lib/i386-linux-gnu/GL/default/lib"

# --- Compat i386 roots (gives you libGL.so.1, ld-linux.so.2, etc.) ---
add_path_front "/app/lib/i386-linux-gnu"
add_path_front "/usr/lib/i386-linux-gnu"

add_path_back "/app/lib32"


# ---------------------------
# Find an i386 loader we can actually execute
# ---------------------------
LOADER=""
for c in \
  /usr/lib/i386-linux-gnu/ld-linux.so.2 \
  /app/lib/i386-linux-gnu/ld-linux.so.2 \
  /lib/i386-linux-gnu/ld-linux.so.2 \
  /lib/ld-linux.so.2 \
  /usr/lib32/ld-linux.so.2
do
  if [[ -x "$c" ]]; then
    LOADER="$c"
    break
  fi
done

if [[ -z "${LOADER}" ]]; then
  echo "ERROR: i386 loader not found in sandbox." >&2
  echo "You likely need: org.freedesktop.Platform.Compat.i386//${FLATPAK_RUNTIME_VERSION:-24.08}" >&2
  echo "Try (user install): flatpak install --user flathub org.freedesktop.Platform.Compat.i386//24.08" >&2
  echo "Sandbox /usr/lib (snippet):" >&2
  ls -la /usr/lib 2>/dev/null | head -n 80 >&2 || true
  exit 127
fi

# ---------------------------
# Optional preflight: show what will be loaded (before running)
# ---------------------------
: "${NOX_LD_TRACE:=0}"   # 1 = print resolved libs via ld.so
: "${NOX_LD_DEBUG:=0}"   # 1 = very verbose loader debug into files

if [[ "${NOX_LD_TRACE}" != "0" ]]; then
  echo "== loader ==" >&2
  echo "LOADER=${LOADER}" >&2
  echo "LIBPATH=${LIBPATH}" >&2
  echo "PKG_NOXD=${PKG_NOXD}" >&2
  echo "SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-}" >&2
  echo >&2

  echo "== ld.so --verify ==" >&2
  "${LOADER}" --verify "${PKG_NOXD}" 2>&1 | sed 's/^/[verify] /' >&2 || true
  echo >&2

  echo "== ld.so --list (resolved DT_NEEDED) ==" >&2
  "${LOADER}" --library-path "${LIBPATH}" --list "${PKG_NOXD}" 2>&1 | sed 's/^/[list] /' >&2 || true
  echo >&2

  echo "== GL32 mount probe ==" >&2
  for p in \
    /app/lib/i386-linux-gnu/GL/default/lib \
    /usr/lib/i386-linux-gnu/GL/default/lib \
    /app/lib/i386-linux-gnu \
    /usr/lib/i386-linux-gnu
  do
    if [[ -d "$p" ]]; then
      echo "[gl] OK: $p" >&2
      ls -la "$p" 2>/dev/null | head -n 30 >&2 || true
    else
      echo "[gl] MISSING: $p" >&2
    fi
  done
  echo >&2
fi

if [[ "${NOX_LD_DEBUG}" != "0" ]]; then
  export LD_DEBUG="libs,files"
  export LD_DEBUG_OUTPUT="${NOX_ASSET_DIR}/ld-debug"
  echo "== LD_DEBUG enabled: output -> ${LD_DEBUG_OUTPUT}.* ==" >&2
fi


# NOX_LIMIT_RANGE_ON_RUN - useful for gamepads and steam deck
# limits the range of the mouse when running but only if starting close to center or passing through center
: "${NOX_LIMIT_RANGE_ON_RUN:=1}"
export NOX_LIMIT_RANGE_ON_RUN
: "${NOX_LIMIT_RANGE_ON_RUN_RADIUS:=118}"
export NOX_LIMIT_RANGE_ON_RUN_RADIUS

: "${NOX_GAMEPAD:=1}"
export NOX_GAMEPAD

: "${NOX_GAMEPAD_LOG:=1}"
export NOX_GAMEPAD_LOG

export # for debug

"${LOADER}" --library-path "${LIBPATH}" "${PKG_NOXD}"
GAME_RC=$?
exit "${GAME_RC}"
