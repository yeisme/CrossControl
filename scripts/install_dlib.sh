#!/usr/bin/env bash
set -euo pipefail

VERSION=${1:-v19.24.8}
ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
DEST="$ROOT_DIR/third_party/dlib"
PRESET=${2:-gcc-cpu-release}

echo "Cloning dlib $VERSION into $DEST"
mkdir -p "$DEST"
pushd "$DEST" >/dev/null
if [ ! -d ".git" ]; then
    git init .
    git remote add origin https://github.com/davisking/dlib.git
    git fetch --depth 1 origin "$VERSION"
    git checkout FETCH_HEAD
else
    git fetch --depth 1 origin "$VERSION"
    git checkout FETCH_HEAD
fi

echo "Configuring and building dlib with CMake preset: $PRESET"
cmake --preset "$PRESET"
cmake --build --preset "$PRESET"
# cmake --install expects the project binary directory, not a preset name
binaryDir="$DEST/build/$PRESET"
echo "Installing from binary dir: $binaryDir"
cmake --install "$binaryDir"

echo "dlib installed to: $DEST/install/dlib/$PRESET"
popd >/dev/null
