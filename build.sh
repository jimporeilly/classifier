#!/bin/bash
set -e
echo "=== Camera Classifier Build Script ==="

# Auto-detect libtorch location
if [ -d "/opt/libtorch" ]; then
    LIBTORCH="/opt/libtorch"
elif [ -d "$HOME/libtorch" ]; then
    LIBTORCH="$HOME/libtorch"
else
    echo "Error: libtorch not found at /opt/libtorch or ~/libtorch"
    exit 1
fi
echo "Using libtorch at: $LIBTORCH"

# Auto-detect opencv include path
if pkg-config --exists opencv4; then
    OPENCV_CFLAGS=$(pkg-config --cflags opencv4)
    OPENCV_LIBS=$(pkg-config --libs opencv4)
else
    echo "Error: opencv4 not found via pkg-config"
    exit 1
fi

mkdir -p build

g++ -g -std=c++17 src/*.cpp -o build/camera_classifier \
  -Iinclude \
  $OPENCV_CFLAGS \
  -I${LIBTORCH}/include \
  -I${LIBTORCH}/include/torch/csrc/api/include \
  -D_GLIBCXX_USE_CXX11_ABI=1 \
  -L/usr/lib/x86_64-linux-gnu \
  -L${LIBTORCH}/lib \
  -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_imgcodecs -lopencv_videoio \
  -ltorch -lc10 -ltorch_cpu -lpthread \
  -Wl,-rpath,${LIBTORCH}/lib \
  $OPENCV_LIBS

echo "=== Build Complete: build/camera_classifier ==="
