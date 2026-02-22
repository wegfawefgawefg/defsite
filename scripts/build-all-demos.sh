#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

make build

for src_dir in demos/*/src; do
  [[ -d "$src_dir" ]] || continue
  demo_name="$(basename "$(dirname "$src_dir")")"
  out_dir="$ROOT_DIR/generated/$demo_name"
  mkdir -p "$out_dir"
  find "$out_dir" -mindepth 1 -delete
  ./bin/defsite "$ROOT_DIR/$src_dir" "$out_dir"
  echo "Built demo '$demo_name': $src_dir -> $out_dir"
done
