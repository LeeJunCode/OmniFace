cmake_minimum_required(VERSION 3.22)

project(OmniFace
    VERSION 1.0.0
    DESCRIPTION "Real-time face detection, tracking, and recognition on CPU"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# vcpkg toolchain should be passed via -DCMAKE_TOOLCHAIN_FILE
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs highgui videoio dnn calib3d)
find_package(onnxruntime REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(spdlog REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets)

# Detect MSVC for platform-specific settings
if(MSVC)
    add_compile_options(/W3 /utf-8 /wd4828)
    add_compile_definitions(NOMINMAX)  # Prevent min/max macros from windows.h
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# OpenFace — facial behavior analysis (landmarks, gaze, AUs)
add_subdirectory(third_party/OpenFace)

add_subdirectory(src)
add_subdirectory(ui)
add_subdirectory(app)
