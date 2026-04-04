#!/usr/bin/env bash
set -euo pipefail

APP_ID="io.github.sookyboo.nox-decomp"
APP_BUNDLE="io.github.sookyboo.nox-decomp.flatpak"

RUNTIMES=(
  "org.freedesktop.Platform//25.08"
  "org.freedesktop.Platform.Compat.i386//25.08"
  "org.freedesktop.Platform.GL32.default//25.08"
)

install_flatpaks() {
  flatpak remote-add --user --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
  flatpak install --user -y flathub "${RUNTIMES[@]}"
  flatpak install --user -y --reinstall "$APP_BUNDLE"
}

# GUI is "present" if we have Wayland or X11 display vars
have_gui() {
  [[ -n "${WAYLAND_DISPLAY:-}" || -n "${DISPLAY:-}" ]]
}

have_zenity() {
  [[ -x /usr/bin/zenity ]] || command -v zenity >/dev/null 2>&1
}

ask_run_now() {
  # Returns 0 if user chose "Run", non-zero otherwise
  /usr/bin/zenity \
    --question \
    --title="Nox-Decomp" \
    --text="Flatpak installed successfully.\n\nRun Nox-Decomp now?" \
    --ok-label="Run" \
    --cancel-label="Not now"
}

run_app() {
  flatpak run "$APP_ID"
}

main() {
  install_flatpaks

  # No GUI: do not attempt to run, just exit successfully.
  if ! have_gui; then
    echo "No GUI detected (DISPLAY/WAYLAND_DISPLAY not set). Installed flatpaks; not running."
    exit 0
  fi

  # GUI present but no zenity: don't run; just inform.
  if ! have_zenity; then
    echo "GUI detected but zenity not found/executable. Installed flatpaks; not running."
    exit 0
  fi

  if ask_run_now; then
    run_app
  else
    echo "User chose not to run now."
  fi
}

main "$@"