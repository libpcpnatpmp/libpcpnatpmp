#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
SMOKE_ROOT="${1:-$ROOT_DIR/build-example-find-package}"
LIB_BUILD_DIR="$SMOKE_ROOT/lib-build"
INSTALL_PREFIX="$SMOKE_ROOT/install"
EXAMPLE_BUILD_DIR="$SMOKE_ROOT/example-build"

# 1) Build and install the library to an isolated local prefix.
#    This mimics how downstream projects consume an installed package.
cmake -S "$ROOT_DIR" -B "$LIB_BUILD_DIR" -DBUILD_TESTS=OFF
cmake --build "$LIB_BUILD_DIR"
cmake --install "$LIB_BUILD_DIR" --prefix "$INSTALL_PREFIX"

# 2) Build the example as a standalone consumer via find_package().
#    CMAKE_PREFIX_PATH points CMake to the installation from step 1.
cmake -S "$SCRIPT_DIR" -B "$EXAMPLE_BUILD_DIR" -DCMAKE_PREFIX_PATH="$INSTALL_PREFIX"
cmake --build "$EXAMPLE_BUILD_DIR" --target pcp-example

echo "Built: $EXAMPLE_BUILD_DIR/pcp-example"
