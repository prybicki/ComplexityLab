#!/bin/bash
# Build dependencies installation script for ComplexityLab
# Run this once after cloning the repository

set -euo pipefail

SLANG_VERSION="2025.24.2"
SLANG_INSTALL_DIR="/usr/local"

echo "=== ComplexityLab Dependencies Installer ==="

if ! command -v sudo &> /dev/null; then
    echo "Error: sudo is required but not installed."
    exit 1
fi

echo ""
echo "=== Installing System Packages ==="

sudo apt-get update
sudo apt-get install -y \
    curl \
    libvulkan-dev vulkan-validationlayers \
    libglfw3-dev libspdlog-dev libgtest-dev libyaml-cpp-dev \
    libavcodec-dev libavformat-dev libavutil-dev \
    libwayland-dev libxkbcommon-dev \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
    libgl-dev
# curl                        — downloads Slang below.
# libvulkan-dev, glfw, spdlog, gtest, yaml-cpp
#                             — resolved by find_package() in cmake/Dependencies.cmake.
#                               (hlslpp replaces glm and is fetched by CMake, not apt.)
# libav* (ffmpeg)             — h.265/MKV recording, resolved there via pkg_check_modules.
# wayland, xkbcommon, x11, gl — GLFW's windowing backends and headers.

echo ""
echo "=== Installing Slang Shader Compiler $SLANG_VERSION ==="

# slangc uses -v, not --version.
if command -v slangc &> /dev/null && [[ "$(slangc -v 2>&1)" == *"$SLANG_VERSION"* ]]; then
    echo "Slang $SLANG_VERSION is already installed."
else
    SLANG_URL="https://github.com/shader-slang/slang/releases/download/v$SLANG_VERSION/slang-$SLANG_VERSION-linux-x86_64.tar.gz"

    TEMP_DIR=$(mktemp -d)
    trap 'rm -rf "$TEMP_DIR"' EXIT

    # The tarball unpacks to a flat bin/ include/ lib/ layout.
    echo "Downloading $SLANG_URL..."
    curl -L --progress-bar "$SLANG_URL" | tar -xz -C "$TEMP_DIR"

    echo "Installing to $SLANG_INSTALL_DIR (requires sudo)..."
    sudo cp -v "$TEMP_DIR/bin/slangc" "$SLANG_INSTALL_DIR/bin/"
    sudo cp -v "$TEMP_DIR/lib/"*.so* "$SLANG_INSTALL_DIR/lib/"
    sudo ldconfig
fi

echo ""
echo "=== Verifying Installation ==="

if ! command -v slangc &> /dev/null; then
    echo "slangc: NOT FOUND - installation may have failed"
    exit 1
fi
echo "slangc: $(command -v slangc) ($(slangc -v 2>&1))"

echo ""
echo "=== Installation Complete ==="
echo "You can now build ComplexityLab with:"
echo "  cmake -B build/debug -DCMAKE_BUILD_TYPE=Debug && cmake --build build/debug -j"
