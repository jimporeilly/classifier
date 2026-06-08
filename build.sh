#!/bin/bash
set -e
echo "=== Camera Classifier Build Script ==="

# Auto-detect libtorch location
if [ -d "/opt/libtorch" ]; then
    TORCH_LIB="/opt/libtorch"
elif [ -d "$HOME/libtorch" ]; then
    TORCH_LIB="$HOME/libtorch"
elif [ -d "$HOME/.local/lib/python3.10/site-packages/torch" ]; then
    TORCH_LIB="$HOME/.local/lib/python3.10/site-packages/torch"
else
    echo "Error: libtorch not found"
    exit 1
fi
echo "Using libtorch at: $TORCH_LIB"

if pkg-config --exists opencv4; then
    OPENCV_CFLAGS=$(pkg-config --cflags opencv4)
    OPENCV_LIBS=$(pkg-config --libs opencv4)
else
    echo "Error: opencv4 not found"
    exit 1
fi

mkdir -p build

g++ -g -std=c++17 src/*.cpp -o build/camera_classifier \
  -Iinclude \
  $OPENCV_CFLAGS \
  -I${TORCH_LIB}/include \
  -I${TORCH_LIB}/include/torch/csrc/api/include \
  -D_GLIBCXX_USE_CXX11_ABI=1 \
  -L/usr/lib/aarch64-linux-gnu \
  -L${TORCH_LIB}/lib \
  -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_imgcodecs -lopencv_videoio \
  -ltorch_cpu -lc10 -lpthread \
  -Wl,-rpath,${TORCH_LIB}/lib \
  -Wl,--allow-shlib-undefined \
  $OPENCV_LIBS

echo "=== Build Complete: build/camera_classifier ==="
