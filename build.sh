#!/bin/bash

# Build script for camera_classifier application

set -e

echo "=== Camera Classifier Build Script ==="

# Check for required environment variables
if [ -z "$LIBTORCH_PATH" ]; then
    echo "Error: LIBTORCH_PATH environment variable not set"
    echo "Please set it to your libtorch installation directory:"
    echo "  export LIBTORCH_PATH=/path/to/libtorch"
    exit 1
fi

# Create build directory
BUILD_DIR="build"
if [ -d "$BUILD_DIR" ]; then
    echo "Cleaning existing build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo "Configuring with CMake..."
cmake -DCMAKE_PREFIX_PATH="$LIBTORCH_PATH" ..

# Build
echo "Building..."
make -j$(nproc)

echo ""
echo "=== Build Complete ==="
echo "Executable location: $BUILD_DIR/camera_classifier"
echo ""
echo "To run:"
echo "  cd $BUILD_DIR"
echo "  ./camera_classifier --camera 0"
echo ""
echo "To run with a model:"
echo "  ./camera_classifier --camera 0 --model /path/to/model.pt"
echo ""
