cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(camera_classifier)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-D_GLIBCXX_USE_CXX11_ABI=1)

# Find packages
find_package(Torch REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui imgcodecs videoio)
find_package(Threads REQUIRED)
find_package(X11 REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${TORCH_INCLUDE_DIRS})

# Link directories MUST come BEFORE add_executable
link_directories(/usr/lib/x86_64-linux-gnu)
link_directories(${OpenCV_LIBRARY_DIRS})

# Add executable
add_executable(camera_classifier
    src/main.cpp
    src/camera_thread.cpp
    src/classifier_thread.cpp
    src/correlation_thread.cpp
    src/gui_handler.cpp
    src/nano_trigger.cpp
)

# Link libraries (FIXED: OpenCV_LIBRARIES not OpenCV_LIBS)
target_link_libraries(camera_classifier
    ${OpenCV_LIBRARIES}
    ${TORCH_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${X11_LIBRARIES}
)


set_property(TARGET camera_classifier PROPERTY CXX_STANDARD 17)
