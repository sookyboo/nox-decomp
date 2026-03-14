#!/usr/bin/env bash
set -euo pipefail

APP_ID=io.github.sookyboo.nox-decomp
MANIFEST=io.github.sookyboo.nox-decomp.yml

rm -rf build-dir repo
ostree --repo=repo init --mode=archive-z2
ostree --repo=repo config set core.min-free-space-percent 0

flatpak-builder --force-clean --repo=repo build-dir "$MANIFEST"
flatpak build-update-repo repo
flatpak build-bundle repo "${APP_ID}.flatpak" "$APP_ID"