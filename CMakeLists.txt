cmake_minimum_required(VERSION 3.22...4.4)

# Default vcpkg toolchain — override via -D if needed
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "C:/Users/34341/dev/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain file")
endif()
set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "vcpkg triplet")

project(OmniFace
    VERSION 1.0.0
    DESCRIPTION "Real-time face detection, tracking, and recognition on CPU/GPU"
    LANGUAGES CXX
)

# ── Qt Multimedia backend ──────────────────────────────────────────
# Qt 6 on Windows defaults to WMF; FFmpeg is recommended for better
# seek accuracy, zero-copy texture transfer, and wider codec support.
set(OMNIFACE_MEDIA_BACKEND "ffmpeg" CACHE STRING "Qt Multimedia backend (ffmpeg | windowsmedia)")
add_compile_definitions(OMNIFACE_MEDIA_BACKEND="${OMNIFACE_MEDIA_BACKEND}")

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

# Qt 6 — prebuilt at C:\Qt\6.11.1\msvc2022_64 (not from vcpkg)
# Use list(APPEND) to preserve vcpkg paths already in CMAKE_PREFIX_PATH
list(APPEND CMAKE_PREFIX_PATH "C:/Qt/6.11.1/msvc2022_64")

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs highgui videoio dnn calib3d)
find_package(Eigen3 REQUIRED)
find_package(spdlog REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core Gui Widgets Sql
    Multimedia MultimediaWidgets
    Charts Svg SvgWidgets
    Concurrent
)

qt_standard_project_setup()
# Detect MSVC for platform-specific settings
if(MSVC)
    add_compile_options(/W3 /utf-8 /wd4828)
    add_compile_definitions(NOMINMAX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# OpenFace — facial behavior analysis (landmarks, gaze, AUs)
add_subdirectory(third_party/OpenFace)

add_subdirectory(src)
