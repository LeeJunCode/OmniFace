cmake_minimum_required(VERSION 3.21)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(omniface VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(FetchContent)

FetchContent_Declare(yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG        0.8.0)
FetchContent_MakeAvailable(yaml-cpp)

FetchContent_Declare(httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.18.3)
FetchContent_MakeAvailable(httplib)

find_package(onnxruntime REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs highgui videoio calib3d dnn)
find_package(Eigen3 REQUIRED)
find_package(OpenBLAS REQUIRED)
add_library(openblas_lib SHARED IMPORTED)
set_target_properties(openblas_lib PROPERTIES
    IMPORTED_LOCATION "${OpenBLAS_LIBRARIES}"
    INTERFACE_INCLUDE_DIRECTORIES "${OpenBLAS_INCLUDE_DIRS}")

find_package(dlib REQUIRED)
find_package(Boost COMPONENTS filesystem system QUIET)
if(NOT Boost_FOUND)
    set(Boost_LIBRARIES stdc++fs)
endif()

set(OF_ROOT "${CMAKE_SOURCE_DIR}/third_party/OpenFace")
set(OF_LIB_DIR "${OF_ROOT}/build/lib/local")
set(OF_INC_BASE "${OF_ROOT}/lib/local")

add_library(of_landmark STATIC IMPORTED)
set_target_properties(of_landmark PROPERTIES
    IMPORTED_LOCATION "${OF_LIB_DIR}/LandmarkDetector/libLandmarkDetector.a")
target_include_directories(of_landmark INTERFACE
    "${OF_INC_BASE}/LandmarkDetector/include"
    "${OF_INC_BASE}/Utilities/include")
target_link_libraries(of_landmark INTERFACE
    opencv_core opencv_imgproc opencv_calib3d
    openblas_lib dlib ${Boost_LIBRARIES})

find_library(OPENCV_OBJDETECT_LIB opencv_objdetect PATHS /usr/lib/x86_64-linux-gnu)
if(OPENCV_OBJDETECT_LIB)
    target_link_libraries(of_landmark INTERFACE ${OPENCV_OBJDETECT_LIB})
endif()

add_library(of_face_analyser STATIC IMPORTED)
set_target_properties(of_face_analyser PROPERTIES
    IMPORTED_LOCATION "${OF_LIB_DIR}/FaceAnalyser/libFaceAnalyser.a")
target_include_directories(of_face_analyser INTERFACE
    "${OF_INC_BASE}/FaceAnalyser/include"
    "${OF_INC_BASE}/LandmarkDetector/include"
    "${OF_INC_BASE}/Utilities/include")
target_link_libraries(of_face_analyser INTERFACE
    opencv_core opencv_imgproc opencv_calib3d openblas_lib dlib ${Boost_LIBRARIES})

add_library(of_gaze STATIC IMPORTED)
set_target_properties(of_gaze PROPERTIES
    IMPORTED_LOCATION "${OF_LIB_DIR}/GazeAnalyser/libGazeAnalyser.a")
target_include_directories(of_gaze INTERFACE
    "${OF_INC_BASE}/GazeAnalyser/include"
    "${OF_INC_BASE}/LandmarkDetector/include")

add_library(of_utils STATIC IMPORTED)
set_target_properties(of_utils PROPERTIES
    IMPORTED_LOCATION "${OF_LIB_DIR}/Utilities/libUtilities.a")
target_include_directories(of_utils INTERFACE
    "${OF_INC_BASE}/Utilities/include"
    "${OF_INC_BASE}/LandmarkDetector/include"
    "${OF_INC_BASE}/FaceAnalyser/include"
    "${OF_INC_BASE}/GazeAnalyser/include")
target_link_libraries(of_utils INTERFACE
    opencv_core opencv_imgproc opencv_imgcodecs opencv_highgui opencv_videoio
    ${Boost_LIBRARIES})

add_subdirectory(src)
add_subdirectory(app)
