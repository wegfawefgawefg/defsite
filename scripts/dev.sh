#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_DIR="${1:-$ROOT_DIR/demos/site/src}"
OUT_DIR="${2:-$ROOT_DIR/generated/site}"
PORT="${PORT:-8000}"

cd "$ROOT_DIR"

./scripts/build.sh "$SRC_DIR" "$OUT_DIR"

python3 -m http.server "$PORT" --directory "$OUT_DIR" >/tmp/templater-dev-server.log 2>&1 &
SERVER_PID=$!

echo "Dev server: http://localhost:$PORT"
echo "Watching: $SRC_DIR"

action_build() {
  if ./scripts/build.sh "$SRC_DIR" "$OUT_DIR"; then
    echo "[dev] rebuild ok"
  else
    echo "[dev] rebuild failed"
  fi
}

cleanup() {
  if kill -0 "$SERVER_PID" >/dev/null 2>&1; then
    kill "$SERVER_PID" >/dev/null 2>&1 || true
  fi
}

trap cleanup EXIT INT TERM

if command -v inotifywait >/dev/null 2>&1; then
  while inotifywait -r -e modify,create,delete,move "$SRC_DIR" >/dev/null 2>&1; do
    action_build
  done
else
  echo "inotifywait not found; using polling fallback (1s)."
  last_hash=""
  while true; do
    new_hash="$(find "$SRC_DIR" -type f -print0 | sort -z | xargs -0 sha1sum | sha1sum | awk '{print $1}')"
    if [[ "$new_hash" != "$last_hash" ]]; then
      if [[ -n "$last_hash" ]]; then
        action_build
      fi
      last_hash="$new_hash"
    fi
    sleep 1
  done
fi
