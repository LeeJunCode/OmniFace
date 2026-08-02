cmake_minimum_required(VERSION 3.22)

# Default vcpkg toolchain and Qt prefix — override via -D if needed
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "C:/Users/34341/dev/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain file")
endif()
set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "vcpkg triplet")
set(CMAKE_PREFIX_PATH "C:/Qt/6.11.1/msvc2022_64" CACHE STRING "Qt6 installation prefix")

project(OmniFace
    VERSION 1.0.0
    DESCRIPTION "Real-time face detection, tracking, and recognition on CPU/GPU"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- ONNX Runtime — local GPU package or vcpkg ----
set(OMNIFACE_ONNXRUNTIME_ROOT "${CMAKE_SOURCE_DIR}/third_party/onnxruntime-win-x64-gpu-1.23.2"
    CACHE PATH "Path to local ONNX Runtime SDK (set to empty to use vcpkg)")

if(OMNIFACE_ONNXRUNTIME_ROOT)
    message(STATUS "Using local ONNX Runtime: ${OMNIFACE_ONNXRUNTIME_ROOT}")
    add_library(onnxruntime SHARED IMPORTED)
    set_target_properties(onnxruntime PROPERTIES
        IMPORTED_LOCATION "${OMNIFACE_ONNXRUNTIME_ROOT}/lib/onnxruntime.dll"
        IMPORTED_IMPLIB   "${OMNIFACE_ONNXRUNTIME_ROOT}/lib/onnxruntime.lib"
    )
    target_include_directories(onnxruntime INTERFACE "${OMNIFACE_ONNXRUNTIME_ROOT}/include")
    add_library(onnxruntime::onnxruntime ALIAS onnxruntime)
    add_compile_definitions(OMNIFACE_HAS_CUDA)
else()
    find_package(onnxruntime REQUIRED)
endif()

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs highgui videoio dnn calib3d)
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
