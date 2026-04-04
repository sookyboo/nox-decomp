#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "Usage: $0 <tag-name> [commit] "
  echo "e.g. v0.9.3-rc2"
  exit 1
fi

TAG_NAME="$1"
COMMIT="${2:-HEAD}"

git rev-parse --verify "$COMMIT" >/dev/null 2>&1 || {
  echo "Error: commit '$COMMIT' not found"
  exit 1
}

echo "Tagging '$TAG_NAME' at '$COMMIT'..."
git tag -f "$TAG_NAME" "$COMMIT"

echo "Pushing tag '$TAG_NAME' to origin..."
git push origin -f "$TAG_NAME"

echo "Done."