APP_ID=io.github.sookyboo.NoxDecomp
MANIFEST=io.github.sookyboo.NoxDecomp.yml

rm -rf build-dir repo
flatpak-builder --force-clean --repo=repo build-dir "$MANIFEST"
flatpak build-update-repo repo

# Optional: create a single-file bundle
flatpak build-bundle repo ${APP_ID}.flatpak "$APP_ID"