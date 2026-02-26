#!/usr/bin/env bash
set -euo pipefail

APP_ID="io.github.sookyboo.nox-decomp"

PKG_BASE="/app/share/nox-decomp"
PKG_NOXD="/app/bin/noxd.i386"
PKG_FFMPEG_DIR="$/app/bin/ffmpeg.i386"
PKG_GPTK2_INI="${PKG_BASE}/nox.gptk2.ini"
PKG_INNOEXTRACT="/app/bin/innoextract"
PKG_FFMPEG_X64="/app/bin/ffmpeg"
PKG_NOX_CFG="${PKG_BASE}/nox.cfg"

# ---------------------------
# Optional Steam integration prompt (zenity + python3)
# ---------------------------

STEAM_SHORTCUT_PY="${PKG_BASE}/steam_shortcut.py"
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
#: "${SDL_GAMECONTROLLERCONFIG_FILE:=${PKG_STEAM_DIR}/gamecontrollerdb.txt}"
: "${SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD:=1}"
#DEFAULT_IGNORE_DEVICES="0x2808/0x1015,0x054c/0x0df2,0x054c/0x0df2,0x045e/0x02e3,0x045e/0x0b00,0x045e/0x0b05,0x2dc8/0x6000,0x2dc8/0x6100,0x2dc8/0x6001,0x2dc8/0x6101,0x2dc8/0x6003,0x2dc8/0x6006,0x2dc8/0x6009,0x2dc8/0x6012,0x28de/0x1002,0x28de/0x1003,0x28de/0x1071,0x28de/0x1052,0x28de/0x1042,0x28de/0x1203,0x28de/0x1204,0x28de/0x1205,0x28de/0x1206,0x28de/0x1302,0x28de/0x1303,0x28de/0x1304,0x28de/0x1305,0x0f0d/0x01ab,0x0f0d/0x0196,0x28de/0x12ff,0x28de/0x12fe,0x28de/0x12fd,0x28de/0x12fc,0x28de/0x12fb,0x28de/0x12fa,0x28de/0x12f9,0x28de/0x12f8,0x28de/0x12f7,0x28de/0x12f6,0x28de/0x12f5,0x28de/0x12f4,0x28de/0x12f3,0x28de/0x12f2,0x28de/0x12f1,0x28de/0x12f0,0x0079/0x181a,0x044f/0xb315,0x044f/0xd007,0x046d/0xcad1,0x054c/0x0268,0x056e/0x200f,0x056e/0x2013,0x05b8/0x1004,0x05b8/0x1006,0x06a3/0xf622,0x0738/0x3180,0x0738/0x3250,0x0738/0x3481,0x0738/0x8180,0x0738/0x8838,0x0810/0x0001,0x0810/0x0003,0x0925/0x0005,0x0925/0x8866,0x0925/0x8888,0x0e6f/0x0109,0x0e6f/0x011e,0x0e6f/0x0128,0x0e6f/0x0214,0x0e6f/0x1314,0x0e6f/0x6302,0x0e8f/0x0008,0x0e8f/0x3075,0x0e8f/0x310d,0x0f0d/0x0009,0x0f0d/0x004d,0x0f0d/0x005f,0x0f0d/0x006a,0x0f0d/0x006e,0x0f0d/0x0085,0x0f0d/0x0086,0x0f0d/0x0088,0x0f30/0x1100,0x11ff/0x3331,0x1345/0x1000,0x1345/0x6005,0x146b/0x5500,0x1a34/0x0836,0x20bc/0x5500,0x20d6/0x576d,0x20d6/0xca6d,0x2563/0x0523,0x2563/0x0575,0x25f0/0x83c3,0x25f0/0xc121,0x2c22/0x2003,0x2c22/0x2302,0x2c22/0x2502,0x8380/0x0003,0x8888/0x0308,0x0079/0x181b,0x044f/0xd00e,0x054c/0x05c4,0x054c/0x05c5,0x054c/0x09cc,0x054c/0x0ba0,0x0738/0x8250,0x0738/0x8384,0x0738/0x8480,0x0738/0x8481,0x0c12/0x0e10,0x0c12/0x0e13,0x0c12/0x0e15,0x0c12/0x0e20,0x0c12/0x0ef6,0x0c12/0x1cf6,0x0c12/0x1e10,0x0c12/0x2e18,0x0e6f/0x0203,0x0e6f/0x0207,0x0e6f/0x020a,0x0f0d/0x0055,0x0f0d/0x005e,0x0f0d/0x0066,0x0f0d/0x0084,0x0f0d/0x0087,0x0f0d/0x008a,0x0f0d/0x009c,0x0f0d/0x00a0,0x0f0d/0x00ee,0x0f0d/0x011c,0x0f0d/0x0123,0x0f0d/0x0162,0x11c0/0x4001,0x146b/0x0d01,0x146b/0x0d02,0x146b/0x0d06,0x146b/0x0d08,0x146b/0x0d09,0x146b/0x0d10,0x146b/0x0d10,0x146b/0x0d13,0x146b/0x1103,0x1532/0x0401,0x1532/0x1000,0x1532/0x1004,0x1532/0x1007,0x1532/0x1008,0x1532/0x1009,0x1532/0x100a,0x1532/0x1100,0x20d6/0x792a,0x2c22/0x2000,0x2c22/0x2300,0x2c22/0x2500,0x3285/0x0d16,0x3285/0x0d17,0x7545/0x0104,0x9886/0x0025,0x054c/0x0ce6,0x054c/0x0df2,0x054c/0x0e5f,0x0e6f/0x0209,0x0f0d/0x0163,0x0f0d/0x0184,0x1532/0x100b,0x1532/0x100c,0x1532/0x1012,0x3285/0x0d18,0x3285/0x0d19,0x358a/0x0104,0x0079/0x18d4,0x03eb/0xff02,0x044f/0xb326,0x045e/0x028e,0x045e/0x028f,0x045e/0x0291,0x045e/0x02a0,0x045e/0x02a1,0x045e/0x02a9,0x045e/0x0719,0x046d/0xc21d,0x046d/0xc21e,0x046d/0xc21f,0x046d/0xc242,0x056e/0x2004,0x0738/0x4716,0x0738/0x4718,0x0738/0x4726,0x0738/0x4728,0x0738/0x4736,0x0738/0x4738,0x0738/0x4740,0x0738/0xb726,0x0738/0xbeef,0x0738/0xcb02,0x0738/0xcb03,0x0738/0xf738,0x0955/0x7210,0x0955/0xb400,0x0b05/0x1b4c,0x0e6f/0x0105,0x0e6f/0x0113,0x0e6f/0x011f,0x0e6f/0x0125,0x0e6f/0x0127,0x0e6f/0x0131,0x0e6f/0x0133,0x0e6f/0x0143,0x0e6f/0x0147,0x0e6f/0x0201,0x0e6f/0x0213,0x0e6f/0x021f,0x0e6f/0x0301,0x0e6f/0x0313,0x0e6f/0x0314,0x0e6f/0x0401,0x0e6f/0x0413,0x0e6f/0x0501,0x0e6f/0xf900,0x0f0d/0x000a,0x0f0d/0x000c,0x0f0d/0x000d,0x0f0d/0x0016,0x0f0d/0x001b,0x0f0d/0x008c,0x0f0d/0x00db,0x0f0d/0x011e,0x1038/0x1430,0x1038/0x1431,0x1038/0xb360,0x11c9/0x55f0,0x12ab/0x0004,0x12ab/0x0301,0x12ab/0x0303,0x1430/0x02a0,0x1430/0x4748,0x1430/0xf801,0x146b/0x0601,0x15e4/0x3f00,0x15e4/0x3f0a,0x15e4/0x3f10,0x162e/0xbeef,0x1689/0xfd00,0x1689/0xfd01,0x1689/0xfe00,0x1949/0x041a,0x1bad/0x0002,0x1bad/0x0003,0x1bad/0xf016,0x1bad/0xf018,0x1bad/0xf019,0x1bad/0xf021,0x1bad/0xf023,0x1bad/0xf025,0x1bad/0xf027,0x1bad/0xf028,0x1bad/0xf02e,0x1bad/0xf036,0x1bad/0xf038,0x1bad/0xf039,0x1bad/0xf03a,0x1bad/0xf03d,0x1bad/0xf03e,0x1bad/0xf03f,0x1bad/0xf042,0x1bad/0xf080,0x1bad/0xf501,0x1bad/0xf502,0x1bad/0xf503,0x1bad/0xf504,0x1bad/0xf505,0x1bad/0xf506,0x1bad/0xf900,0x1bad/0xf901,0x1bad/0xf902,0x1bad/0xf903,0x1bad/0xf904,0x1bad/0xf906,0x1bad/0xfa01,0x1bad/0xfd00,0x1bad/0xfd01,0x24c6/0x5000,0x24c6/0x5300,0x24c6/0x5303,0x24c6/0x530a,0x24c6/0x531a,0x24c6/0x5397,0x24c6/0x5500,0x24c6/0x5501,0x24c6/0x5502,0x24c6/0x5503,0x24c6/0x5506,0x24c6/0x550d,0x24c6/0x550e,0x24c6/0x5508,0x24c6/0x5510,0x24c6/0x5b00,0x24c6/0x5b02,0x24c6/0x5b03,0x24c6/0x5d04,0x24c6/0xfafa,0x24c6/0xfafb,0x24c6/0xfafc,0x24c6/0xfafd,0x24c6/0xfafe,0x03f0/0x0495,0x044f/0xd012,0x045e/0x02d1,0x045e/0x02dd,0x045e/0x02e0,0x045e/0x02e3,0x045e/0x02ea,0x045e/0x02fd,0x045e/0x02ff,0x045e/0x0b00,0x045e/0x0b05,0x045e/0x0b0a,0x045e/0x0b0c,0x045e/0x0b12,0x045e/0x0b13,0x045e/0x0b20,0x045e/0x0b21,0x045e/0x0b22,0x0738/0x4a01,0x0e6f/0x0139,0x0e6f/0x013b,0x0e6f/0x013a,0x0e6f/0x0145,0x0e6f/0x0146,0x0e6f/0x015b,0x0e6f/0x015c,0x0e6f/0x015d,0x0e6f/0x015f,0x0e6f/0x0160,0x0e6f/0x0161,0x0e6f/0x0162,0x0e6f/0x0163,0x0e6f/0x0164,0x0e6f/0x0165,0x0e6f/0x0166,0x0e6f/0x0167,0x0e6f/0x0205,0x0e6f/0x0206,0x0e6f/0x0246,0x0e6f/0x0261,0x0e6f/0x0262,0x0e6f/0x02a0,0x0e6f/0x02a1,0x0e6f/0x02a2,0x0e6f/0x02a3,0x0e6f/0x02a4,0x0e6f/0x02a5,0x0e6f/0x02a6,0x0e6f/0x02a7,0x0e6f/0x02a8,0x0e6f/0x02a9,0x0e6f/0x02aa,0x0e6f/0x02ab,0x0e6f/0x02ac,0x0e6f/0x02ad,0x0e6f/0x02ae,0x0e6f/0x02af,0x0e6f/0x02b0,0x0e6f/0x02b1,0x0e6f/0x02b3,0x0e6f/0x02b5,0x0e6f/0x02b6,0x0e6f/0x02bd,0x0e6f/0x02be,0x0e6f/0x02bf,0x0e6f/0x02c0,0x0e6f/0x02c1,0x0e6f/0x02c2,0x0e6f/0x02c3,0x0e6f/0x02c4,0x0e6f/0x02c5,0x0e6f/0x02c6,0x0e6f/0x02c7,0x0e6f/0x02c8,0x0e6f/0x02c9,0x0e6f/0x02ca,0x0e6f/0x02cb,0x0e6f/0x02cd,0x0e6f/0x02ce,0x0e6f/0x02cf,0x0e6f/0x02d5,0x0e6f/0x0346,0x0e6f/0x0446,0x0e6f/0x02da,0x0e6f/0x02d6,0x0e6f/0x02d9,0x0f0d/0x0063,0x0f0d/0x0067,0x0f0d/0x0078,0x0f0d/0x00c5,0x0f0d/0x0150,0x10f5/0x7009,0x10f5/0x7013,0x1532/0x0a00,0x1532/0x0a03,0x1532/0x0a14,0x1532/0x0a15,0x20d6/0x2001,0x20d6/0x2002,0x20d6/0x2003,0x20d6/0x2004,0x20d6/0x2005,0x20d6/0x2006,0x20d6/0x2009,0x20d6/0x200a,0x20d6/0x200b,0x20d6/0x200c,0x20d6/0x200d,0x20d6/0x200e,0x20d6/0x200f,0x20d6/0x2011,0x20d6/0x2012,0x20d6/0x2015,0x20d6/0x2016,0x20d6/0x2017,0x20d6/0x2018,0x20d6/0x2019,0x20d6/0x201a,0x20d6/0x4001,0x20d6/0x4002,0x20d6/0x890b,0x24c6/0x541a,0x24c6/0x542a,0x24c6/0x543a,0x24c6/0x551a,0x24c6/0x561a,0x24c6/0x581a,0x24c6/0x591a,0x24c6/0x592a,0x24c6/0x791a,0x2dc8/0x2002,0x2dc8/0x3106,0x2dc8/0x310a,0x2e24/0x0652,0x2e24/0x1618,0x2e24/0x1688,0x146b/0x0611,0x045e/0x02a2,0x0e6f/0x1414,0x0e6f/0x0159,0x24c6/0xfaff,0x0f0d/0x006d,0x0f0d/0x00a4,0x0079/0x1832,0x0079/0x187f,0x0079/0x1883,0x03eb/0xff01,0x0c12/0x0ef8,0x046d/0x1000,0x11ff/0x0511,0x1345/0x6006,0x056e/0x2012,0x146b/0x0602,0x0f0d/0x00ae,0x046d/0x0401,0x046d/0x0301,0x046d/0xcaa3,0x046d/0xc261,0x046d/0x0291,0x0079/0x18d3,0x0f0d/0x00b1,0x0079/0x188e,0x0079/0x187c,0x0079/0x189c,0x0079/0x1874,0x2f24/0x0050,0x2f24/0x002e,0x2f24/0x0091,0x1430/0x0719,0x0f0d/0x00ed,0x0f0d/0x00c0,0x0e6f/0x0152,0x046d/0x1007,0x0e6f/0x02b8,0x0079/0x18a1,0x0000/0x6686,0x12ab/0x0304,0x1430/0x0291,0x1430/0x02a9,0x1430/0x070b,0x1bad/0x028e,0x1bad/0x02a0,0x1bad/0x5500,0x20ab/0x55ef,0x24c6/0x5509,0x2516/0x0069,0x25b1/0x0360,0x2c22/0x2203,0x2f24/0x0011,0x2f24/0x0053,0x2f24/0x00b7,0x046d/0x0000,0x046d/0x1004,0x046d/0x1008,0x046d/0xf301,0x0738/0x02a0,0x0738/0x7263,0x0738/0xb738,0x0738/0xcb29,0x0738/0xf401,0x0079/0x18c2,0x0079/0x18c8,0x0079/0x18cf,0x0c12/0x0e17,0x0c12/0x0e1c,0x0c12/0x0e22,0x0c12/0x0e30,0xd2d2/0xd2d2,0x0d62/0x9a1a,0x0d62/0x9a1b,0x0e00/0x0e00,0x0e6f/0x012a,0x0e6f/0x02b2,0x0f0d/0x0097,0x0f0d/0x00ba,0x0f0d/0x00d8,0x0fff/0x02a1,0x045e/0x0867,0x16d0/0x0f3f,0x2f24/0x008f,0x0e6f/0xf501,0x057e/0x2006,0x057e/0x2067,0x057e/0x2007,0x057e/0x2066,0x057e/0x2008,0x057e/0x2068,0x057e/0x2009,0x057e/0x2069,0x0f0d/0x00c1,0x0f0d/0x0092,0x0f0d/0x00f6,0x0e6f/0x0180,0x0e6f/0x0181,0x0e6f/0x0184,0x0e6f/0x0185,0x0e6f/0x0186,0x0e6f/0x0187,0x0e6f/0x0188,0x0e6f/0x018c,0x0f0d/0x00aa,0x20d6/0xa711,0x20d6/0xa712,0x20d6/0xa713,0x20d6/0xa714,0x20d6/0xa715,0x20d6/0xa716,0x20d6/0xa718,0x33dd/0x0001,0x33dd/0x0002,0x33dd/0x0003,0x0f0d/0x00f0,0x0000/0x11fb,0x28de/0x1101,0x28de/0x1102,0x28de/0x1105,0x28de/0x1106,0x28de/0x1142,0x28de/0x1201,0x28de/0x1202,0x28de/0x1205,0x28de/0x1302,0x28de/0x1303,0x28de/0x1304,0x28de/0x1305,0x2dc8/0x3019,0x2dc8/0x3019,0x2dc8/0x9000,0x2dc8/0x3810,0x2dc8/0x0651,0x2dc8/0x9020,0x2dc8/0x9015,0x2dc8/0x2865,0x1235/0xab12,0x2002/0x9000,0x2dc8/0x9001,0x3820/0x0009,0x2dc8/0x3820,0x2810/0x0009,0x2dc8/0x2830,0x2dc8/0x6002,0x2dc8/0x6102,0x1235/0xab20,0x2820/0x0009,0x2dc8/0x301b,0x2dc8/0x3011,0x2dc8/0x3013,0x2dc8/0x9018,0x2dc8/0x3230,0x05a0/0x3232,0x05a0/0x3232,0x2dc8/0x3100,0x2dc8/0x9012,0x2dc8/0x2862,0x0b05/0x4500,0x0b05/0x4500,0x0b05/0x7905,0x0b05/0x7906,0x0010/0x0082,0x1949/0x0402,0x1949/0x0419,0x0171/0x0419,0x0079/0x1830,0x3250/0x1001,0x3250/0x1001,0x3250/0x1002,0x3250/0x1002,0x24c6/0x891b,0x0c12/0x0ef7,0x04b4/0x010a,0xffff/0xffff,0x20e8/0x5860,0x0926/0x8888,0x0e6f/0x0130,0x0079/0x0011,0x0000/0x0000,0x1a34/0xf705,0x1949/0x0402,0x3537/0x1097,0x05ac/0x061a,0x0000/0x0000,0x25f0/0x83c1,0x18d1/0x9400,0x18d1/0x9400,0x0428/0x4001,0x0e8f/0x1006,0x0e8f/0x0012,0x0f0d/0x0010,0x0f0d/0x0022,0x0f0d/0x006b,0xdead/0xbeef,0x14d8/0x6208,0x0e8f/0x3013,0x04d8/0x0082,0x05fd/0x3000,0x1949/0x0402,0x056e/0x2003,0x0f30/0x0110,0x22ba/0x1020,0x046d/0xc219,0x046d/0xc216,0x046d/0xc216,0x046d/0xc219,0x046d/0xc218,0x046d/0xc211,0x24c6/0x892b,0x24c6/0x892a,0x24c6/0x891a,0x0738/0x5266,0x0738/0x3384,0x0738/0x3480,0x0738/0x8818,0x0078/0x0006,0x045e/0x000e,0x20d6/0x0dad,0x146b/0x0c01,0x0810/0xe501,0x0955/0x7214,0x0955/0x7214,0x124b/0x4d01,0x1345/0x3008,0x0079/0x1843,0x0079/0x1844,0x057e/0x2019,0x057e/0x2019,0x057e/0x201e,0x057e/0x2017,0x057e/0x2017,0x057e/0x2017,0x0000/0x0000,0x057e/0x0306,0x057e/0x0330,0x057e/0x0306,0x0001/0x0001,0x050d/0x0803,0x2836/0x0001,0x2836/0x0001,0x11ff/0x3341,0x0e8f/0x0003,0x0000/0x0000,0x054c/0x0cda,0x0f30/0x1112,0x2c22/0x2012,0x2c22/0x2010,0x1532/0x0402,0x1532/0x0705,0x1532/0x0900,0x1532/0x0900,0xf000/0x0003,0x0079/0x0011,0x1a34/0x0809,0x7545/0x1122,0x06a3/0xf623,0x06a3/0xff0c,0x06a3/0x040c,0x06a3/0x0109,0x06a3/0x040b,0x06a3/0xf518,0x16c0/0x0487,0x28de/0x11fc,0x0111/0x1431,0x0111/0x1419,0x6666/0x8804,0xf000/0x00f1,0x044f/0xb320,0x044f/0xb323,0x044f/0xb300,0x044f/0xd009,0x044f/0xd008,0x12bd/0xd015,0x14d8/0xcd07,0x0079/0x0011,0x05ac/0x3232,0x0c45/0x4320,0x0000/0x0000,0x0000/0x0000,0x2717/0x3144,0x16c0/0x05e1,0x3507/0x0004,0x0079/0x0122,0x6666/0x0667,0x0583/0x2060,0x0000/0x0000,0x07b5/0x0315,0x289b/0x0080,0x289b/0x0003"
#: "${SDL_GAMECONTROLLER_IGNORE_DEVICES:=${DEFAULT_IGNORE_DEVICES}}"
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

# --- Bundled 32-bit ffmpeg (if you ship it) ---
add_path_back "${PKG_FFMPEG_DIR}"

# --- Steam SDL (32-bit) ---
# Put this late-ish so GL and other runtime libs aren’t accidentally shadowed.
# If you *must* force Steam SDL first, flip add_path_back -> add_path_front.
#add_path_back "${PKG_STEAM_DIR}/libs.i386.x11"

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
