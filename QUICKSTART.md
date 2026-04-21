# Quick Start Guide

## Installation (5 minutes)

```bash
# 1. Run the setup script
./setup.sh

# 2. Set environment variables (add to ~/.bashrc for persistence)
export LIBTORCH_PATH=/path/to/libtorch
export LD_LIBRARY_PATH=$LIBTORCH_PATH/lib:$LD_LIBRARY_PATH

# 3. Build the project
./build.sh
```

## Running Without a Model

```bash
cd build
./camera_classifier --camera 0
```

This will:
- Open your USB camera
- Display live video feed
- Show the control panel
- Work without classification (good for testing camera)

## Running With a Model

```bash
# 1. Export a pre-trained model (one-time)
./export_model.py --model resnet18 --output models/model.pt

# 2. Run with the model
cd build
./camera_classifier --camera 0 --model ../models/model.pt
```

## Controls

### Keyboard
- **ESC** - Exit
- **P** - Pause/Resume
- **C** - Capture frame

### GUI Buttons
- **Pause/Resume** - Toggle processing
- **Capture Frame** - Save current frame
- **Clear Queue** - Clear processing queue
- **Exit** - Close application

### Slider
- **Confidence** - Minimum confidence threshold (0-100%)

## Common Issues

### Camera not found
```bash
# List available cameras
ls -l /dev/video*

# Try different camera IDs
./camera_classifier --camera 1
```

### LibTorch not found
```bash
# Make sure environment variables are set
export LIBTORCH_PATH=/path/to/libtorch
export LD_LIBRARY_PATH=$LIBTORCH_PATH/lib:$LD_LIBRARY_PATH

# Or add to ~/.bashrc for persistence
echo 'export LIBTORCH_PATH=/path/to/libtorch' >> ~/.bashrc
echo 'export LD_LIBRARY_PATH=$LIBTORCH_PATH/lib:$LD_LIBRARY_PATH' >> ~/.bashrc
source ~/.bashrc
```

### Build errors
```bash
# Clean and rebuild
rm -rf build
./build.sh
```

## VSCode Setup

1. Open project folder in VSCode
2. Install recommended extensions:
   - C/C++ (Microsoft)
   - CMake Tools
3. Press F5 to build and debug

## Next Steps

- Train your own model and export to TorchScript
- Customize class names in `src/main.cpp`
- Adjust preprocessing in `src/classifier_thread.cpp`
- Modify GUI in `src/gui_handler.cpp`

For detailed documentation, see README.md
