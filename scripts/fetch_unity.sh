#!/usr/bin/env bash
# Fetch ThrowTheSwitch Unity for host unit tests.
#
#   ./scripts/fetch_unity.sh [dest]
# Dest defaults to $ROOT/.deps/unity

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${1:-${UNITY_PATH:-$ROOT/.deps/unity}}"
PARENT="$(dirname "$DEST")"

mkdir -p "$PARENT"

if [[ ! -d "$DEST/.git" ]]; then
  git clone --depth 1 --branch v2.6.0 \
    https://github.com/ThrowTheSwitch/Unity.git "$DEST"
fi

test -f "$DEST/src/unity.c"
test -f "$DEST/src/unity.h"
echo "UNITY_PATH=$DEST"
echo "Unity fetch OK"
