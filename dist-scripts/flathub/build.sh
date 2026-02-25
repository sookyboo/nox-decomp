APP_ID=io.github.sookyboo.nox-decomp
MANIFEST=io.github.sookyboo.nox-decomp.yml

rm -rf build-dir repo
flatpak-builder --force-clean --repo=repo build-dir "$MANIFEST"
flatpak build-update-repo repo

# Optional: create a single-file bundle
flatpak build-bundle repo ${APP_ID}.flatpak "$APP_ID"