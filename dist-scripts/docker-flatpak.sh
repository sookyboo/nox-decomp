#!/usr/bin/env bash
set -euo pipefail

IMAGE_NAME="${IMAGE_NAME:-noxdecomp-flatpak-builder}"
DOCKERFILE="${DOCKERFILE:-Dockerfile.flatpak}"
WORKDIR="${WORKDIR:-$PWD}"

# Extra args you can pass through, e.g.
#   EXTRA_RUN_ARGS="--pull=always" ./docker-flatpak.sh
EXTRA_BUILD_ARGS="${EXTRA_BUILD_ARGS:-}"
EXTRA_RUN_ARGS="${EXTRA_RUN_ARGS:-}"

echo "== Building image =="
echo "  IMAGE_NAME = ${IMAGE_NAME}"
echo "  DOCKERFILE = ${DOCKERFILE}"
echo "  WORKDIR    = ${WORKDIR}"
echo

docker build \
  -f "${DOCKERFILE}" \
  -t "${IMAGE_NAME}" \
  ${EXTRA_BUILD_ARGS} \
  .

echo
echo "== Running build in container =="
echo "  Mounting ${WORKDIR} -> /work"
echo

# Flatpak/bwrap in Docker typically needs:
# - privileged (or at least wide-ranging caps + unconfined seccomp/apparmor)
# - access to /dev/fuse (sometimes)
# - host cgroups/IPC often help (varies by host)
#
# This uses the "make it work" settings. You can tighten later if you want.
docker run --rm -it \
  --privileged \
  --security-opt seccomp=unconfined \
  --security-opt apparmor=unconfined \
  --device /dev/fuse \
  --tmpfs /tmp:exec \
  --tmpfs /run \
  -v "${WORKDIR}:/work" \
  -w /work \
  ${EXTRA_RUN_ARGS} \
  "${IMAGE_NAME}"
