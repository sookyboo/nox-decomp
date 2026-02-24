#!/usr/bin/env bash
set -euo pipefail

APP_NAME="${APP_NAME:-NoxDecomp}"
RUNTIME_VER="${RUNTIME_VER:-24.08}"
GL_VER="${GL_VER:-1.4}"
APP_ID="${APP_ID:-io.github.sookyboo.NoxDecomp}"
ARCH="${ARCH:-x86_64}"

WORKDIR="$(pwd)"
BUILD_DIR="${BUILD_DIR:-build-dir}"
REPO_DIR="${REPO_DIR:-repo}"
OUT_DIR="${OUT_DIR:-out}"
LOCAL_REMOTE_NAME="${LOCAL_REMOTE_NAME:-noxdecomp-local}"

echo "== Config =="
echo "  APP_ID      = ${APP_ID}"
echo "  RUNTIME_VER = ${RUNTIME_VER}"
echo "  GL_VER      = ${GL_VER}"
echo "  ARCH        = ${ARCH}"
echo "  WORKDIR     = ${WORKDIR}"
echo "  BUILD_DIR   = ${BUILD_DIR}"
echo "  REPO_DIR    = ${REPO_DIR}"
echo "  OUT_DIR     = ${OUT_DIR}"
echo

# ---------------------------
# Prereqs (system flathub + runtime/sdk + i386 compat + GL32)
# ---------------------------
if ! sudo flatpak remotes --system | awk '{print $1}' | grep -qx flathub; then
  sudo flatpak remote-add --if-not-exists --system flathub https://dl.flathub.org/repo/flathub.flatpakrepo
fi

# Base runtime/sdk (x86_64)
sudo flatpak install -y --system --noninteractive --no-related --arch="${ARCH}" flathub \
  "org.freedesktop.Platform//${RUNTIME_VER}" \
  "org.freedesktop.Sdk//${RUNTIME_VER}"

# i386 compat runtime extension (still installed as x86_64 “extension ref”)
sudo flatpak install -y --system --noninteractive --no-related --arch="${ARCH}" flathub \
  "org.freedesktop.Platform.Compat.i386//${RUNTIME_VER}"

# GL32 extension is typically “…GL32.default” for the runtime version
# (ok if already installed / not available on this host – keep || true)
sudo flatpak install -y --system --noninteractive --no-related --arch="${ARCH}" flathub \
  "org.freedesktop.Platform.GL32.default//${RUNTIME_VER}" || true

# ---------------------------
# Verify inputs
# ---------------------------
echo "== Verifying inputs in ${WORKDIR} =="
ls -la "${WORKDIR}"

test -f "${WORKDIR}/start.sh"
test -f "${WORKDIR}/noxd.i386"
test -d "${WORKDIR}/steam"
[[ -d "${WORKDIR}/ffmpeg.i386" ]] || echo "WARN: ffmpeg.i386 missing (continuing)"
[[ -f "${WORKDIR}/utils/innoextract.x86_64" ]] || echo "WARN: utils/innoextract.x86_64 missing (extract step won't work)"
[[ -f "${WORKDIR}/utils/ffmpeg.x86_64" ]]     || echo "WARN: utils/ffmpeg.x86_64 missing (dialog convert step won't work)"

# ---------------------------
# Clean & init
# ---------------------------
echo "== Cleaning previous outputs =="
rm -rf "${BUILD_DIR}" "${REPO_DIR}" "${OUT_DIR}"
rm -f "${OUT_DIR}/${APP_ID}.flatpak"

echo "== flatpak build-init =="
flatpak build-init --arch="${ARCH}" "${BUILD_DIR}" "${APP_ID}" \
  org.freedesktop.Sdk org.freedesktop.Platform "${RUNTIME_VER}"

# ---------------------------
# Staging into /app/lib/$APP_ID
# ---------------------------
echo "== Staging files =="
INTERNAL_LIB="${BUILD_DIR}/files/lib/${APP_ID}"
#install -d "${INTERNAL_LIB}" "${BUILD_DIR}/files/bin" "${BUILD_DIR}/files/share/metainfo"
install -d "${INTERNAL_LIB}" \
  "${BUILD_DIR}/files/bin" \
  "${BUILD_DIR}/files/share/metainfo" \
  "${BUILD_DIR}/files/share/applications" \
  "${BUILD_DIR}/files/share/icons/hicolor/256x256/apps" \
  "${BUILD_DIR}/files/share/icons/hicolor/128x128/apps" \
  "${BUILD_DIR}/files/share/icons/hicolor/64x64/apps"

# IMPORTANT: create extension mount points inside /app (prevents bwrap mkdir RO failure)
install -d "${BUILD_DIR}/files/lib/i386-linux-gnu"
install -d "${BUILD_DIR}/files/lib/i386-linux-gnu/GL"

install -Dm755 "${WORKDIR}/noxd.i386" "${INTERNAL_LIB}/noxd.i386"
install -Dm755 "${WORKDIR}/start.sh" "${BUILD_DIR}/files/bin/start.sh"

# Tools (innoextract + ffmpeg) staged into /app/lib/$APP_ID/utils
install -d "${INTERNAL_LIB}/utils"
[[ -f "${WORKDIR}/utils/innoextract.x86_64" ]] && install -Dm755 "${WORKDIR}/utils/innoextract.x86_64" "${INTERNAL_LIB}/utils/innoextract.x86_64"
[[ -f "${WORKDIR}/utils/ffmpeg.x86_64" ]]     && install -Dm755 "${WORKDIR}/utils/ffmpeg.x86_64"     "${INTERNAL_LIB}/utils/ffmpeg.x86_64"

# steam integration
install -Dm755 "${WORKDIR}/steam_shortcut.py" "${INTERNAL_LIB}/steam_shortcut.py"
install -Dm644 "${WORKDIR}/steamv.png" "${INTERNAL_LIB}/steamv.png"
install -Dm644 "${WORKDIR}/steamh.png" "${INTERNAL_LIB}/steamh.png"
install -Dm644 "${WORKDIR}/io.github.sookyboo.NoxDecomp.png" "${INTERNAL_LIB}/${APP_ID}.png"

# ---------------------------
# Desktop entry (auto-exported by Flatpak)
# ---------------------------
cat > "${BUILD_DIR}/files/share/applications/${APP_ID}.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=${APP_NAME}
Exec=start.sh
Icon=${APP_ID}
Categories=Game;
Terminal=false
EOF
#Exec=${APP_ID}

if [[ -f "${WORKDIR}/io.github.sookyboo.NoxDecomp.png" ]]; then
  install -Dm644 "${WORKDIR}/io.github.sookyboo.NoxDecomp.png" "${BUILD_DIR}/files/share/icons/hicolor/256x256/apps/${APP_ID}.png"
  install -Dm644 "${WORKDIR}/io.github.sookyboo.NoxDecomp.png" "${BUILD_DIR}/files/share/icons/hicolor/128x128/apps/${APP_ID}.png"
  install -Dm644 "${WORKDIR}/io.github.sookyboo.NoxDecomp.png" "${BUILD_DIR}/files/share/icons/hicolor/64x64/apps/${APP_ID}.png"
else
  echo "WARN: io.github.sookyboo.NoxDecomp.png missing; desktop entry will have no icon."
fi

[[ -f "${WORKDIR}/gptokeyb2.x86_64" ]] && install -Dm755 "${WORKDIR}/gptokeyb2.x86_64" "${INTERNAL_LIB}/gptokeyb2.x86_64"
[[ -f "${WORKDIR}/nox.cfg" ]]          && install -Dm644 "${WORKDIR}/nox.cfg" "${INTERNAL_LIB}/nox.cfg"
[[ -f "${WORKDIR}/nox.gptk2.ini" ]]    && install -Dm644 "${WORKDIR}/nox.gptk2.ini" "${INTERNAL_LIB}/nox.gptk2.ini"

cp -a "${WORKDIR}/steam" "${INTERNAL_LIB}/steam"
[[ -d "${WORKDIR}/ffmpeg.i386" ]] && cp -a "${WORKDIR}/ffmpeg.i386" "${INTERNAL_LIB}/ffmpeg.i386"

# Minimal metainfo (reduces appstream noise)
cat > "${BUILD_DIR}/files/share/metainfo/${APP_ID}.metainfo.xml" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<component type="desktop-application">
  <id>${APP_ID}</id>
  <name>NoxDecomp</name>
  <summary>Nox Decomp wrapper</summary>
  <metadata_license>CC0-1.0</metadata_license>
  <project_license>Proprietary</project_license>
</component>
EOF

echo "===== STAGED TREE (sanity) ====="
find "${BUILD_DIR}/files" -maxdepth 6 -print
echo "================================"
test -f "${INTERNAL_LIB}/noxd.i386"

# ---------------------------
# Finish (permissions)
# ---------------------------
echo "== flatpak build-finish =="
flatpak build-finish "${BUILD_DIR}" \
  --command=start.sh \
  --allow=multiarch \
  --share=ipc \
  --share=network \
  --socket=fallback-x11 \
  --socket=wayland \
  --device=dri \
  --socket=pulseaudio \
  --filesystem=~/nox-decomp:create \
  --device=all \
  --filesystem=/run/udev:ro \
  --filesystem=xdg-data/Steam:rw \
  --filesystem=~/.steam/steam:rw \
  --filesystem=~/.local/share/Steam:rw \
  --filesystem=~/.var/app/com.valvesoftware.Steam/data/Steam:rw

# ---------------------------
# Inject i386 + GL32 extension metadata (OpenNox-style, but AUTO-DOWNLOAD GL32)
# ---------------------------
echo "== Injecting i386 + GL32 extension metadata =="

META_FILE="${BUILD_DIR}/metadata"

# Strip any previous copies to keep rebuilds clean-ish
tmp="$(mktemp)"
awk '
  BEGIN {skip=0}
  /^\[Extension org\.freedesktop\.Platform\.Compat\.i386\]$/ {skip=1}
  /^\[Extension org\.freedesktop\.Platform\.Compat\.i386\.Debug\]$/ {skip=1}
  /^\[Extension org\.freedesktop\.Platform\.GL32\]$/ {skip=1}
  /^\[Extension / { if (skip==1) {skip=0} }
  { if (skip==0) print }
' "${META_FILE}" > "${tmp}"
cat "${tmp}" > "${META_FILE}"
rm -f "${tmp}"

cat >> "${META_FILE}" <<EOF

[Extension org.freedesktop.Platform.Compat.i386]
directory=lib/i386-linux-gnu
version=${RUNTIME_VER}

[Extension org.freedesktop.Platform.Compat.i386.Debug]
directory=lib/debug/lib/i386-linux-gnu
version=${RUNTIME_VER}
no-autodownload=true

[Extension org.freedesktop.Platform.GL32]
directory=lib/i386-linux-gnu/GL
version=${GL_VER}
versions=${RUNTIME_VER};${GL_VER}
subdirectories=true
# NOTE: we intentionally DO NOT set no-autodownload=true here,
# so Flatpak is allowed to fetch GL32 automatically when applicable.
autodelete=false
add-ld-path=lib
merge-dirs=vulkan/icd.d;glvnd/egl_vendor.d;OpenCL/vendors;lib/dri;lib/d3d;vulkan/explicit_layer.d;vulkan/implicit_layer.d
download-if=active-gl-driver
enable-if=active-gl-driver
EOF

# ---------------------------
# Export + bundle
# ---------------------------
echo "== flatpak build-export =="
mkdir -p "${REPO_DIR}" "${OUT_DIR}"
flatpak build-export --arch="${ARCH}" "${REPO_DIR}" "${BUILD_DIR}"
flatpak build-update-repo "${REPO_DIR}" || true

echo "== VERIFY exported repo refs =="
ostree --repo="${REPO_DIR}" refs

echo "== VERIFY exported commit contains our payload (ostree ls) =="
REF="app/${APP_ID}/${ARCH}/master"
COMMIT="$(ostree --repo="${REPO_DIR}" rev-parse "${REF}")"
ostree --repo="${REPO_DIR}" ls -R "${COMMIT}" /files/lib/"${APP_ID}" | head -n 200
ostree --repo="${REPO_DIR}" ls "${COMMIT}" /files/lib/"${APP_ID}"/noxd.i386 >/dev/null

echo "== flatpak build-bundle =="
flatpak build-bundle --arch="${ARCH}" "${REPO_DIR}" "${OUT_DIR}/${APP_ID}.flatpak" "${APP_ID}"

# ---------------------------
# VERIFY install from local repo (user)
# ---------------------------
echo "== VERIFY install from local repo (user) =="

LOCAL_URL="file://${WORKDIR}/${REPO_DIR}"
if flatpak remotes --user | awk '{print $1}' | grep -qx "${LOCAL_REMOTE_NAME}"; then
  flatpak remote-modify --user --no-gpg-verify --url="${LOCAL_URL}" "${LOCAL_REMOTE_NAME}"
else
  flatpak remote-add --user --no-gpg-verify "${LOCAL_REMOTE_NAME}" "${LOCAL_URL}"
fi

flatpak uninstall -y --user "${APP_ID}" >/dev/null 2>&1 || true

# IMPORTANT: do NOT use --no-deps; we want Flatpak to pull declared deps/extensions.
flatpak install -y --user --noninteractive "${LOCAL_REMOTE_NAME}" "${APP_ID}"

flatpak run --command=sh "${APP_ID}" -c 'find /app -maxdepth 4 \( -type f -o -type d \) -print'
flatpak run --command=sh "${APP_ID}" -c "test -f /app/lib/${APP_ID}/noxd.i386"
flatpak run --command=sh "${APP_ID}" -c "test -d /app/lib/i386-linux-gnu && echo OK:/app/lib/i386-linux-gnu || true"
flatpak run --command=sh "${APP_ID}" -c "find /app/lib/i386-linux-gnu/GL -maxdepth 4 -type d 2>/dev/null | head -n 20 || true"
flatpak run --command=sh "${APP_ID}" -c "test -d /app/lib/${APP_ID}/utils && echo OK:/app/lib/${APP_ID}/utils || true"
flatpak run --command=sh "${APP_ID}" -c "ls -la /app/lib/${APP_ID}/utils 2>/dev/null || true"

echo
echo "✅ Build Complete: ${OUT_DIR}/${APP_ID}.flatpak"
echo "Install: flatpak install -y --user --reinstall ${OUT_DIR}/${APP_ID}.flatpak"
echo "Run:     flatpak run ${APP_ID}"