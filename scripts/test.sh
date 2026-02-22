#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT_DIR/bin/defsite"
TMP_ROOT="$ROOT_DIR/.tmp-test-out-$$"
mkdir -p "$TMP_ROOT"
cleanup() {
  if [[ -d "$TMP_ROOT" ]]; then
    rm -r "$TMP_ROOT"
  fi
}
trap cleanup EXIT

pass_count=0
fail_count=0

assert_patterns() {
  local pattern_file="$1"
  local target_file="$2"
  local case_name="$3"
  local case_kind="$4"

  [[ -f "$pattern_file" ]] || return 0

  while IFS= read -r pattern; do
    [[ -z "$pattern" ]] && continue
    if ! grep -F "$pattern" "$target_file" >/dev/null 2>&1; then
      echo "[FAIL] $case_kind case '$case_name' missing stderr pattern: $pattern"
      return 1
    fi
  done <"$pattern_file"
  return 0
}

run_pass_case() {
  local case_dir="$1"
  local case_name
  case_name="$(basename "$case_dir")"
  local out_dir="$TMP_ROOT/pass-$case_name"
  mkdir -p "$out_dir"

  if ! "$BIN" "$case_dir/input" "$out_dir" >"$TMP_ROOT/$case_name.stdout" 2>"$TMP_ROOT/$case_name.stderr"; then
    echo "[FAIL] pass case '$case_name' exited non-zero"
    fail_count=$((fail_count + 1))
    return
  fi

  if ! diff -ru "$case_dir/expected" "$out_dir" >"$TMP_ROOT/$case_name.diff"; then
    echo "[FAIL] pass case '$case_name' output mismatch"
    cat "$TMP_ROOT/$case_name.diff"
    fail_count=$((fail_count + 1))
    return
  fi

  if ! assert_patterns "$case_dir/stderr_contains.txt" "$TMP_ROOT/$case_name.stderr" "$case_name" "pass"; then
    fail_count=$((fail_count + 1))
    return
  fi

  echo "[OK] pass case '$case_name'"
  pass_count=$((pass_count + 1))
}

run_fail_case() {
  local case_dir="$1"
  local case_name
  case_name="$(basename "$case_dir")"
  local out_dir="$TMP_ROOT/fail-$case_name"
  mkdir -p "$out_dir"

  if "$BIN" "$case_dir/input" "$out_dir" >"$TMP_ROOT/$case_name.stdout" 2>"$TMP_ROOT/$case_name.stderr"; then
    echo "[FAIL] fail case '$case_name' unexpectedly succeeded"
    fail_count=$((fail_count + 1))
    return
  fi

  if ! assert_patterns "$case_dir/error_contains.txt" "$TMP_ROOT/$case_name.stderr" "$case_name" "fail"; then
    fail_count=$((fail_count + 1))
    return
  fi

  echo "[OK] fail case '$case_name'"
  pass_count=$((pass_count + 1))
}

for case in "$ROOT_DIR"/tests/pass/*; do
  [[ -d "$case" ]] || continue
  run_pass_case "$case"
done

for case in "$ROOT_DIR"/tests/fail/*; do
  [[ -d "$case" ]] || continue
  run_fail_case "$case"
done

if [[ "$fail_count" -ne 0 ]]; then
  echo "Tests failed: $fail_count failure(s), $pass_count passed"
  exit 1
fi

echo "All tests passed: $pass_count case(s)"
