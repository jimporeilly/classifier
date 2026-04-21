#!/bin/bash

# Setup script for Camera Classifier Application
# This script helps install dependencies and configure the environment

set -e

echo "=== Camera Classifier Setup Script ==="
echo ""

# Check if running on Ubuntu/Debian
if ! command -v apt &> /dev/null; then
    echo "Warning: This script is designed for Ubuntu/Debian systems"
    echo "You may need to manually install dependencies"
fi

# Install system dependencies
echo "Installing system dependencies..."
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    git \
    wget \
    unzip \
    libopencv-dev \
    python3 \
    python3-pip

echo ""
echo "Checking installed versions..."
echo "CMake: $(cmake --version | head -n1)"
echo "OpenCV: $(pkg-config --modversion opencv4 2>/dev/null || echo 'Not found')"
echo "GCC: $(gcc --version | head -n1)"

# Check for LibTorch
echo ""
echo "Checking for LibTorch..."
if [ -z "$LIBTORCH_PATH" ]; then
    echo "LIBTORCH_PATH not set. Would you like to download LibTorch? (y/n)"
    read -r response
    if [[ "$response" =~ ^[Yy]$ ]]; then
        echo "Choose LibTorch version:"
        echo "1) CPU only (smaller download)"
        echo "2) CUDA 11.8 (requires NVIDIA GPU)"
        echo "3) CUDA 12.1 (requires NVIDIA GPU)"
        read -r choice
        
        case $choice in
            1)
                TORCH_URL="https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-2.1.0%2Bcpu.zip"
                TORCH_FILE="libtorch-cpu.zip"
                ;;
            2)
                TORCH_URL="https://download.pytorch.org/libtorch/cu118/libtorch-cxx11-abi-shared-with-deps-2.1.0%2Bcu118.zip"
                TORCH_FILE="libtorch-cu118.zip"
                ;;
            3)
                TORCH_URL="https://download.pytorch.org/libtorch/cu121/libtorch-cxx11-abi-shared-with-deps-2.1.0%2Bcu121.zip"
                TORCH_FILE="libtorch-cu121.zip"
                ;;
            *)
                echo "Invalid choice"
                exit 1
                ;;
        esac
        
        echo "Downloading LibTorch..."
        wget -O "$TORCH_FILE" "$TORCH_URL"
        
        echo "Extracting..."
        unzip -q "$TORCH_FILE"
        rm "$TORCH_FILE"
        
        LIBTORCH_PATH="$(pwd)/libtorch"
        echo ""
        echo "LibTorch installed to: $LIBTORCH_PATH"
        echo ""
        echo "Add this to your ~/.bashrc:"
        echo "export LIBTORCH_PATH=$LIBTORCH_PATH"
        echo "export LD_LIBRARY_PATH=\$LIBTORCH_PATH/lib:\$LD_LIBRARY_PATH"
    fi
else
    echo "LibTorch found at: $LIBTORCH_PATH"
fi

# Install Python dependencies for model export
echo ""
echo "Installing Python dependencies..."
pip3 install torch torchvision --upgrade

# Make build script executable
chmod +x build.sh
chmod +x export_model.py

echo ""
echo "=== Setup Complete ==="
echo ""
echo "Next steps:"
echo "1. Set environment variables (if not already done):"
echo "   export LIBTORCH_PATH=/path/to/libtorch"
echo "   export LD_LIBRARY_PATH=\$LIBTORCH_PATH/lib:\$LD_LIBRARY_PATH"
echo ""
echo "2. Build the project:"
echo "   ./build.sh"
echo ""
echo "3. (Optional) Export a test model:"
echo "   ./export_model.py --model resnet18 --output models/model.pt"
echo ""
echo "4. Run the application:"
echo "   cd build"
echo "   ./camera_classifier --camera 0 --model ../models/model.pt"
echo ""
