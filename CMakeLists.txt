cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(camera_classifier)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-D_GLIBCXX_USE_CXX11_ABI=1)
# Auto-detect Torch location

set(CUDA_TOOLKIT_ROOT_DIR "/usr/local/cuda-12.6")
set(CMAKE_CUDA_COMPILER "/usr/local/cuda-12.6/bin/nvcc")

if(EXISTS "$ENV{HOME}/.local/lib/python3.10/site-packages/torch/share/cmake/Torch")
    set(CMAKE_PREFIX_PATH "$ENV{HOME}/.local/lib/python3.10/site-packages/torch")
elseif(EXISTS "/opt/libtorch/share/cmake/Torch")
    set(CMAKE_PREFIX_PATH "/opt/libtorch")
elseif(EXISTS "$ENV{HOME}/libtorch/share/cmake/Torch")
    set(CMAKE_PREFIX_PATH "$ENV{HOME}/libtorch")
endif()

# Find packages
find_package(Torch REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui imgcodecs videoio)
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${TORCH_INCLUDE_DIRS})

# Link directories MUST come BEFORE add_executable

link_directories(/usr/lib/aarch64-linux-gnu /usr/lib/x86_64-linux-gnu)
link_directories(${OpenCV_LIBRARY_DIRS})

# Add executable
add_executable(camera_classifier
    src/main.cpp
    src/camera_thread.cpp
    src/classifier_thread.cpp
    src/correlation_thread.cpp
    src/gui_handler.cpp
)

# Link libraries (FIXED: OpenCV_LIBRARIES not OpenCV_LIBS)
get_target_property(TORCH_LIBS_LIST torch INTERFACE_LINK_LIBRARIES)
list(REMOVE_ITEM TORCH_LIBS_LIST 
    "${TORCH_LIB_DIR}/libtorch_cuda.so"
    "torch_cuda"
)

target_link_libraries(camera_classifier
    ${OpenCV_LIBRARIES}
    torch_cpu
    c10
    ${CMAKE_THREAD_LIBS_INIT}
    -Wl,--allow-shlib-undefined
)


set_property(TARGET camera_classifier PROPERTY CXX_STANDARD 17)
