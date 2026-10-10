#!/bin/bash
# make-package.sh - Assemble the Miyoo A30 (spruceOS) zip.
#
# Usage: ./a30/make-package.sh [path/to/AnimalCrossing] [output.zip]
#   binary default: <repo>/pc/build-a30/bin/AnimalCrossing (pc/build-a30-docker.sh)
#   output default: a30/AnimalCrossing-ac-gc-a30.zip
#
# Zip layout — extract onto the SD card root:
#   Roms/A30PORTS/Animal Crossing.sh
#   Roms/A30PORTS/ac-gc/AnimalCrossing
#   Roms/A30PORTS/ac-gc/shaders/        (canonical copies from pc/shaders/)
#   Roms/A30PORTS/ac-gc/rom/README.txt  (user supplies their own GAFE01 iso)
#   Roms/A30PORTS/ac-gc/LICENSE.txt, licenses.txt
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
BIN="${1:-$REPO/pc/build-a30/bin/AnimalCrossing}"
OUT="${2:-$HERE/AnimalCrossing-ac-gc-a30.zip}"
case "$OUT" in /*) ;; *) OUT="$PWD/$OUT" ;; esac

[ -f "$BIN" ] || { echo "A30 binary not found: $BIN (build with pc/build-a30-docker.sh)"; exit 1; }

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT

PORT="$STAGE/Roms/A30PORTS/ac-gc"
mkdir -p "$PORT/shaders" "$PORT/rom"
cp "$REPO/pc/shaders/"* "$PORT/shaders/"
cp "$HERE/rom-README.txt" "$PORT/rom/README.txt"
cp "$REPO/LICENSE" "$PORT/LICENSE.txt"
cp "$HERE/licenses.txt" "$PORT/licenses.txt"
cp "$BIN" "$PORT/AnimalCrossing"
chmod +x "$PORT/AnimalCrossing"
cp "$HERE/Animal Crossing.sh" "$STAGE/Roms/A30PORTS/Animal Crossing.sh"

rm -f "$OUT"
(cd "$STAGE" && zip -qr "$OUT" .)
echo "Package: $OUT"
unzip -l "$OUT"
