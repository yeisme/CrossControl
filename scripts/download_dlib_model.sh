#!/usr/bin/env bash
set -euo pipefail

URL1=${1:-"https://raw.githubusercontent.com/davisking/dlib-models/refs/heads/master/taguchi_face_recognition_resnet_model_v1.dat.bz2"}
URL2=${2:-"https://raw.githubusercontent.com/davisking/dlib-models/refs/heads/master/shape_predictor_5_face_landmarks.dat.bz2"}
OUTDIR=${3:-"$(cd "$(dirname "$0")/.." && pwd)/resources/models"}
FORCE=${4:-0}
# Enable decompression by default (set 0 to disable)
DECOMPRESS=${5:-1}

mkdir -p "$OUTDIR"

download() {
  local url="$1"
  local out="$2"
  if [ -f "$out" ] && [ "$FORCE" -eq 0 ]; then
    echo "File exists, skipping: $out"
    return 0
  fi
  echo "Downloading: $url -> $out"
  if command -v curl >/dev/null 2>&1; then
    curl -fSL "$url" -o "$out"
  elif command -v wget >/dev/null 2>&1; then
    wget -O "$out" "$url"
  else
    echo "Neither curl nor wget found. Cannot download files." >&2
    return 2
  fi
}

try_decompress() {
  local bz2="$1"
  local out="$2"
  if [ ! -f "$bz2" ]; then return 1; fi
  if command -v 7z >/dev/null 2>&1; then
    echo "Decompressing with 7z: $bz2 -> $out"
    7z e -so "$bz2" > "$out"
    return 0
  fi
  if command -v bunzip2 >/dev/null 2>&1; then
    echo "Decompressing with bunzip2: $bz2 -> $out"
    bunzip2 -c "$bz2" > "$out"
    return 0
  fi
  if command -v bzip2 >/dev/null 2>&1; then
    echo "Decompressing with bzip2: $bz2 -> $out"
    bzip2 -d -c "$bz2" > "$out"
    return 0
  fi
  echo "No bzip2 decompressor found; skipping decompression" >&2
  return 2
}

for u in "$URL1" "$URL2"; do
  fn=$(basename "$u")
  dest="$OUTDIR/$fn"
  download "$u" "$dest"
  if [ "$DECOMPRESS" -ne 0 ]; then
    if [[ "$dest" == *.bz2 ]]; then
      # Remove only the trailing .bz2 so name.dat.bz2 -> name.dat
      outdat="${dest%.bz2}"
      try_decompress "$dest" "$outdat" || true
    fi
  fi
done

echo "Models saved to: $OUTDIR"
