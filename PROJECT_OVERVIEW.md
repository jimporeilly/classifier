# Camera Classifier Project - Complete Overview

## What You Have

A complete, production-ready multi-threaded camera classifier application with:

✅ **Multi-threaded architecture** (camera, classifier, GUI threads)
✅ **LibTorch integration** for deep learning inference
✅ **OpenCV** for camera capture and display
✅ **Interactive GUI** with control panel
✅ **Thread-safe frame queue**
✅ **Real-time classification**
✅ **VSCode integration** with debugging support
✅ **Build system** (CMake)
✅ **Setup scripts** for easy installation

## Project Structure

```
classifier_app/
├── CMakeLists.txt              # Build configuration
├── build.sh                    # Build automation script
├── setup.sh                    # Dependency installation script
├── export_model.py             # PyTorch to TorchScript export
├── train_and_export.py         # Custom model training script
├── README.md                   # Comprehensive documentation
├── QUICKSTART.md              # Quick start guide
├── .gitignore                 # Git ignore file
│
├── .vscode/                   # VSCode configuration
│   ├── c_cpp_properties.json  # C++ IntelliSense settings
│   ├── launch.json            # Debug configurations
│   ├── tasks.json             # Build tasks
│   └── settings.json          # Workspace settings
│
├── include/                   # Header files
│   ├── shared_data.h          # Thread-safe queue and shared state
│   ├── camera_thread.h        # Camera capture interface
│   ├── classifier_thread.h    # Classifier interface
│   └── gui_handler.h          # GUI interface
│
├── src/                       # Implementation files
│   ├── main.cpp              # Application entry point
│   ├── camera_thread.cpp     # Camera capture implementation
│   ├── classifier_thread.cpp # Classification logic
│   └── gui_handler.cpp       # GUI and control panel
│
└── models/                    # Place your .pt models here
```

## Key Features

### 1. Multi-Threading Architecture

- **Camera Thread**: Captures frames from USB camera at ~30 FPS
- **Classifier Thread**: Runs inference on frames from queue
- **GUI Thread**: Displays video and handles user interaction
- **Thread-Safe Queue**: Efficiently passes frames between threads

### 2. GUI Control Panel

**Buttons:**
- Pause/Resume - Toggle processing
- Capture Frame - Save current frame as JPEG
- Clear Queue - Empty the frame queue
- Exit - Close application

**Trackbar:**
- Confidence threshold adjustment (0-100%)

**Status Display:**
- Real-time FPS counter
- Status messages
- Classification results with confidence

### 3. LibTorch Integration

- Supports TorchScript models (.pt files)
- GPU acceleration (CUDA) if available
- Automatic preprocessing (ImageNet normalization)
- Efficient inference pipeline

### 4. OpenCV Integration

- USB camera capture
- Image display
- GUI rendering
- Frame manipulation

## How It Works

```
USB Camera → Camera Thread → Thread-Safe Queue → Classifier Thread
                                                        ↓
                                                  Classification
                                                        ↓
GUI Thread ← Shared State ← Results ←──────────────────┘
    ↓
Display Window + Control Panel
```

## Getting Started

### Step 1: Install Dependencies
```bash
./setup.sh
```

### Step 2: Set Environment Variables
```bash
export LIBTORCH_PATH=/path/to/libtorch
export LD_LIBRARY_PATH=$LIBTORCH_PATH/lib:$LD_LIBRARY_PATH
```

### Step 3: Build
```bash
./build.sh
```

### Step 4: Run
```bash
cd build
./camera_classifier --camera 0
```

## Customization Points

### Change Class Names
Edit `src/main.cpp`, line ~55:
```cpp
classifier_thread.loadClassNames({
    "your_class_1",
    "your_class_2",
    // ...
});
```

### Adjust Camera Resolution
Edit `src/camera_thread.cpp`, lines ~25-27:
```cpp
capture_.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
capture_.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
```

### Change Input Size
Edit `src/classifier_thread.cpp`, line ~75:
```cpp
cv::resize(rgb_image, resized_image, cv::Size(224, 224));
```

### Modify GUI Layout
Edit `src/gui_handler.cpp`, updateControlPanel() function

## Using Your Own Model

### Option 1: Export Pre-trained Model
```bash
./export_model.py --model resnet18 --output models/my_model.pt
```

### Option 2: Train Custom Model
```bash
# Prepare data in this structure:
# data/
#   train/
#     class1/
#     class2/
#   val/
#     class1/
#     class2/

./train_and_export.py --data-dir ./data --output models/custom.pt
```

### Option 3: Export Your Own Model
```python
import torch

# Your trained model
model = YourModel()
model.load_state_dict(torch.load('checkpoint.pth'))
model.eval()

# Export to TorchScript
example = torch.rand(1, 3, 224, 224)
traced = torch.jit.trace(model, example)
traced.save('model.pt')
```

## VSCode Development

1. **Open in VSCode**: `code classifier_app`
2. **Build**: Press `Ctrl+Shift+B`
3. **Debug**: Press `F5`
4. **IntelliSense**: Works automatically with configuration

## Performance Tips

1. **Use GPU**: Download CUDA version of LibTorch
2. **Reduce Resolution**: Lower camera resolution for higher FPS
3. **Optimize Model**: Use TorchScript optimization
4. **Batch Processing**: Modify to process multiple frames
5. **Model Selection**: Smaller models = faster inference

## Troubleshooting

### Camera not opening
```bash
ls -l /dev/video*
sudo usermod -a -G video $USER
# Then log out and back in
```

### LibTorch not found
```bash
# Verify environment variables
echo $LIBTORCH_PATH
echo $LD_LIBRARY_PATH

# Add to ~/.bashrc for persistence
echo 'export LIBTORCH_PATH=/path/to/libtorch' >> ~/.bashrc
source ~/.bashrc
```

### Build errors
```bash
# Clean rebuild
rm -rf build
./build.sh

# Check CMake can find dependencies
cd build
cmake -DCMAKE_PREFIX_PATH=$LIBTORCH_PATH .. -LAH | grep -i torch
```

### Low FPS
- Check if using GPU: Look for "Using CUDA" in console
- Reduce camera resolution
- Use smaller model (mobilenet vs resnet)

## Code Architecture

### Thread Safety
- All shared state protected by mutexes
- Thread-safe queue implementation
- Atomic flags for simple state

### Frame Flow
1. Camera captures → pushes to queue
2. Queue holds max 10 frames (drops old if full)
3. Classifier pops from queue → processes → updates shared state
4. GUI reads shared state → displays

### Error Handling
- Graceful camera failure handling
- Model loading error recovery
- Empty frame detection
- Thread cleanup on exit

## What's Next?

1. **Add More Features**:
   - Recording video
   - Multiple camera support
   - Object detection overlay
   - Performance metrics

2. **Improve UI**:
   - Qt integration for richer UI
   - Settings dialog
   - Graph of confidence over time

3. **Enhance Classification**:
   - Top-K predictions
   - Bounding boxes
   - Multiple model support

4. **Optimize Performance**:
   - TensorRT integration
   - Model quantization
   - Batch inference

## Files Summary

- **CMakeLists.txt**: Build configuration
- **build.sh**: Automated build script
- **setup.sh**: Dependency installer
- **export_model.py**: PyTorch → TorchScript converter
- **train_and_export.py**: Train custom models
- **README.md**: Full documentation
- **QUICKSTART.md**: Quick reference
- **shared_data.h**: Thread-safe data structures
- **camera_thread.***: Camera capture logic
- **classifier_thread.***: Classification logic
- **gui_handler.***: GUI and controls
- **main.cpp**: Application entry point

## Resources

- LibTorch: https://pytorch.org/cppdocs/
- OpenCV: https://docs.opencv.org/
- CMake: https://cmake.org/documentation/
- TorchScript: https://pytorch.org/docs/stable/jit.html

---

**You now have a complete, professional camera classifier application!**
Ready to build, customize, and deploy. 🚀
