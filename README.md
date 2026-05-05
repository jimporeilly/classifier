# Multi-threaded Camera Classifier

A real-time camera classification application using LibTorch and OpenCV with a multi-threaded architecture and interactive GUI control panel.

## Features

- **Multi-threaded Architecture**
  - Separate threads for camera capture, classification, and GUI
  - Thread-safe queue for efficient frame passing
  - Non-blocking operation

- **Real-time Processing**
  - Live USB camera feed
  - Continuous classification with confidence scores
  - FPS monitoring for both camera and inference

- **Interactive GUI**
  - Live camera feed display with classification overlay
  - Control panel with buttons and status messages
  - Adjustable confidence threshold via slider
  - Pause/Resume, Capture Frame, and Clear Queue controls

- **LibTorch Integration**
  - Support for TorchScript models (.pt files)
  - GPU acceleration (CUDA) if available
  - Standard preprocessing pipeline (ImageNet normalization)

## Prerequisites

### System Requirements
- Ubuntu 20.04 or later
- CUDA-capable GPU (optional, for faster inference)
- USB camera

### Required Libraries

1. **LibTorch** (PyTorch C++ API)
   - Download from: https://pytorch.org/get-started/locally/
   - Choose version matching your CUDA version (or CPU-only)

2. **OpenCV 4.x**
   ```bash
   sudo apt update
   sudo apt install libopencv-dev
   ```

3. **CMake**
   ```bash
   sudo apt install cmake build-essential
   ```

## Installation

### 1. Install LibTorch

```bash
# Download LibTorch (CPU version)
wget https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-2.1.0%2Bcpu.zip
unzip libtorch-cxx11-abi-shared-with-deps-2.1.0+cpu.zip
export LIBTORCH_PATH=$(pwd)/libtorch

# For GPU version, download CUDA version instead:
# wget https://download.pytorch.org/libtorch/cu118/libtorch-cxx11-abi-shared-with-deps-2.1.0%2Bcu118.zip
```

Add to your `~/.bashrc`:
```bash
export LIBTORCH_PATH=/path/to/libtorch
```

### 2. Install OpenCV

```bash
sudo apt update
sudo apt install libopencv-dev
```

Verify installation:
```bash
pkg-config --modversion opencv4
```

### 3. Clone/Download Project

Place the project files in your workspace.

## Building

### Using the Build Script

```bash
cd classifier_app
chmod +x build.sh
./build.sh
```

### Manual Build

```bash
mkdir build
cd build
cmake -DCMAKE_PREFIX_PATH=$LIBTORCH_PATH ..
make -j$(nproc)
```

## Usage

### Basic Usage (No Model)

Run without a classification model to test camera feed:

```bash
cd build
./camera_classifier --camera 0
```

### With Classification Model

```bash
./camera_classifier --camera 0 --model /path/to/your/model.pt
```

### Command Line Options

- `--camera <id>` : Camera device ID (default: 0)
- `--model <path>` : Path to TorchScript model file
- `--help` : Show help message

### Keyboard Shortcuts

- **ESC** : Exit application
- **P** : Pause/Resume processing
- **C** : Capture current frame to file

### GUI Controls

The control panel provides:
- **Pause/Resume Button** : Pause or resume processing
- **Capture Frame Button** : Save current frame as JPEG
- **Clear Queue Button** : Clear the frame processing queue
- **Exit Button** : Close the application
- **Confidence Slider** : Adjust minimum confidence threshold (0-100%)
- **Status Box** : Shows current status and FPS
- **Classification Display** : Shows detected class and confidence

## Creating a TorchScript Model

To use with this application, you need to export your PyTorch model to TorchScript format:

```python
import torch
import torchvision.models as models

# Load your trained model
model = models.resnet18(pretrained=True)
model.eval()

# Create example input
example = torch.rand(1, 3, 224, 224)

# Trace the model
traced_script_module = torch.jit.trace(model, example)

# Save the model
traced_script_module.save("model.pt")
```

### Model Requirements

- Input: `[batch_size, 3, 224, 224]` (RGB image)
- Output: `[batch_size, num_classes]` (logits)
- Preprocessing: Applied automatically (ImageNet normalization)

## Project Structure

```
classifier_app/
├── CMakeLists.txt          # Build configuration
├── build.sh                # Build script
├── README.md               # This file
├── include/                # Header files
│   ├── shared_data.h       # Shared data structures and thread-safe queue
│   ├── camera_thread.h     # Camera capture thread
│   ├── classifier_thread.h # Classification thread
│   └── gui_handler.h       # GUI and control panel
├── src/                    # Source files
│   ├── main.cpp           # Main application
│   ├── camera_thread.cpp  # Camera implementation
│   ├── classifier_thread.cpp # Classifier implementation
│   └── gui_handler.cpp    # GUI implementation
└── models/                # Place your .pt models here
```

## Customization

### Changing Class Names

Edit `src/main.cpp` to specify your class names:

```cpp
classifier_thread.loadClassNames({
    "your_class_1",
    "your_class_2",
    "your_class_3"
});
```

### Adjusting Input Size

If your model uses different input dimensions, modify the preprocessing in `src/classifier_thread.cpp`:

```cpp
cv::resize(rgb_image, resized_image, cv::Size(your_width, your_height));
```

### Camera Resolution

Modify in `src/camera_thread.cpp`:

```cpp
capture_.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
capture_.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
```

## Troubleshooting

### Camera Not Opening
- Check camera permissions: `sudo usermod -a -G video $USER`
- Verify camera device: `ls -l /dev/video*`
- Try different camera IDs: `--camera 1`, `--camera 2`

### LibTorch Not Found
```bash
export LIBTORCH_PATH=/path/to/libtorch
export LD_LIBRARY_PATH=$LIBTORCH_PATH/lib:$LD_LIBRARY_PATH
```

### OpenCV Not Found
```bash
sudo apt install libopencv-dev
pkg-config --cflags --libs opencv4
```

### CUDA Errors
- Ensure CUDA version matches LibTorch version
- Check CUDA installation: `nvidia-smi`
- Fallback to CPU version if needed

### Low FPS
- Reduce camera resolution
- Use GPU if available
- Optimize model (quantization, pruning)
- Adjust queue size in `shared_data.h`

## Performance Tips

1. **Use GPU**: Download CUDA-enabled LibTorch for 10-100x faster inference
2. **Model Optimization**: Use TorchScript optimization passes
3. **Reduce Resolution**: Lower camera resolution if high FPS not needed
4. **Batch Processing**: Modify to process multiple frames per inference
5. **Model Selection**: Use smaller models (MobileNet, SqueezeNet) for faster inference

## Example Models

Download pre-trained models:

```bash
# ResNet18 (PyTorch Hub)
# MobileNetV2
# EfficientNet

# Or train your own using PyTorch and export to TorchScript
```

## VSCode Configuration

### Recommended Extensions
- C/C++ (Microsoft)
- CMake Tools
- CMake

### VSCode Settings (.vscode/c_cpp_properties.json)

```json
{
    "configurations": [
        {
            "name": "Linux",
            "includePath": [
                "${workspaceFolder}/**",
                "${env:LIBTORCH_PATH}/include",
                "${env:LIBTORCH_PATH}/include/torch/csrc/api/include",
                "/usr/include/opencv4"
            ],
            "defines": [],
            "compilerPath": "/usr/bin/g++",
            "cStandard": "c17",
            "cppStandard": "c++17",
            "intelliSenseMode": "linux-gcc-x64"
        }
    ],
    "version": 4
}
```

## License

This project is provided as-is for educational and development purposes.

## Contributing

Feel free to submit issues, feature requests, or pull requests.

## Acknowledgments

- PyTorch/LibTorch team
- OpenCV community
- Contributors and testers
