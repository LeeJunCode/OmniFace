# ============================================================
# Detail layer — ONNX Runtime session wrapper + preprocessing
# ============================================================
add_library(omniface_detail STATIC
    core/detail/onnx_session.cc
    core/detail/preprocess.cc
)

target_include_directories(omniface_detail PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(omniface_detail PUBLIC
    onnxruntime::onnxruntime
    spdlog::spdlog
    opencv_core
    opencv_imgproc
    opencv_dnn
)

target_compile_features(omniface_detail PUBLIC cxx_std_20)

# ============================================================
# Detection module — SCRFD face detector
# ============================================================
add_library(omniface_detection STATIC
    core/detection/face_detector.cc
)

target_include_directories(omniface_detection PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(omniface_detection PUBLIC
    omniface_detail
    opencv_core
)

target_link_libraries(omniface_detection PRIVATE
    opencv_dnn
    opencv_imgproc
)

target_compile_features(omniface_detection PUBLIC cxx_std_20)

# ============================================================
# Tracking module — ByteTrack + FaceTracker
# ============================================================
add_library(omniface_tracking STATIC
    core/tracking/bytetrack_engine.cc
    core/tracking/face_tracker.cc
    core/tracking/detail/kalman_filter.cc
    core/tracking/detail/strack.cc
    core/tracking/detail/lapjv.cc
)

target_include_directories(omniface_tracking PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(omniface_tracking PUBLIC
    omniface_detail
    opencv_core
    Eigen3::Eigen
)

target_compile_features(omniface_tracking PUBLIC cxx_std_20)

# ============================================================
# Recognition module — FaceRecognizer + FaceMatcher
# ============================================================
add_library(omniface_recognition STATIC
    core/recognition/face_recognizer.cc
    core/recognition/face_matcher.cc
)

target_include_directories(omniface_recognition PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(omniface_recognition PUBLIC
    omniface_detail
    opencv_core
    opencv_calib3d
)

target_link_libraries(omniface_recognition PRIVATE
    Eigen3::Eigen
    opencv_dnn
    opencv_imgproc
)

target_compile_features(omniface_recognition PUBLIC cxx_std_20)

# ============================================================
# Analysis module — OpenFace wrapper (landmarks, gaze, AUs)
# ============================================================
add_library(omniface_analysis STATIC
    core/analysis/openface/openface_analyzer.cc
    core/analysis/openface/openface_writer.cc
)

target_include_directories(omniface_analysis PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(omniface_analysis PUBLIC
    omniface_detail
    opencv_core
    opencv_imgproc
    OpenFace::LandmarkDetector
    OpenFace::FaceAnalyser
    OpenFace::GazeAnalyser
    OpenFace::Utilities
)

target_compile_features(omniface_analysis PUBLIC cxx_std_20)

# ============================================================
# MediaPipe Analysis module — PoseEstimator + Visualizer + Analyzer
# ============================================================
add_library(omniface_analysis_mediapipe STATIC
    core/analysis/mediapipe/mediapipe_pose_estimator.cc
    core/analysis/mediapipe/mediapipe_visualizer.cc
    core/analysis/mediapipe/mediapipe_analyzer.cc
)

target_include_directories(omniface_analysis_mediapipe PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(omniface_analysis_mediapipe PUBLIC
    omniface_detail
    spdlog::spdlog
    opencv_core
    opencv_imgproc
    opencv_calib3d
)

target_compile_features(omniface_analysis_mediapipe PUBLIC cxx_std_20)

# ============================================================
# Gaze Analysis module — L2CS-Net GazeEstimator
# ============================================================
add_library(omniface_analysis_l2cs STATIC
    core/analysis/l2cs/l2cs_estimator.cc
)

target_include_directories(omniface_analysis_l2cs PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(omniface_analysis_l2cs PUBLIC
    omniface_detail
    spdlog::spdlog
    opencv_core
    opencv_imgproc
)

target_compile_features(omniface_analysis_l2cs PUBLIC cxx_std_20)

# ============================================================
# MEFARG AU Analysis module — Action Unit estimator
# ============================================================
add_library(omniface_analysis_mefarg STATIC
    core/analysis/mefarg_au/mefarg_au_estimator.cc
)

target_include_directories(omniface_analysis_mefarg PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(omniface_analysis_mefarg PUBLIC
    omniface_detail
    spdlog::spdlog
    opencv_core
    opencv_imgproc
)

target_compile_features(omniface_analysis_mefarg PUBLIC cxx_std_20)

# ============================================================
# Pipeline module — config + orchestration
# ============================================================
add_library(omniface_pipeline STATIC
    pipeline/config.cc
    pipeline/decode_worker.cc
    pipeline/module_factory.cc
    pipeline/output_manager.cc
    pipeline/target_tracker.cc
    pipeline/pipeline.cc
)

target_include_directories(omniface_pipeline PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(omniface_pipeline PUBLIC
    omniface_detection
    omniface_tracking
    omniface_recognition
    omniface_analysis
    omniface_analysis_mediapipe
    omniface_analysis_l2cs
    omniface_analysis_mefarg
    yaml-cpp::yaml-cpp
    spdlog::spdlog
    opencv_core
    opencv_imgproc
    opencv_highgui
    opencv_videoio
)

target_compile_features(omniface_pipeline PUBLIC cxx_std_20)
