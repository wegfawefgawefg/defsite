#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_DIR="${1:-$ROOT_DIR/demos/site/src}"
OUT_DIR="${2:-$ROOT_DIR/generated/site}"

cd "$ROOT_DIR"
make build
mkdir -p "$OUT_DIR"
find "$OUT_DIR" -mindepth 1 -delete
./bin/templater "$SRC_DIR" "$OUT_DIR"

echo "Built site: $SRC_DIR -> $OUT_DIR"
