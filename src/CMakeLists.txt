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
# Quality evaluation module — post-processing CSV analysis
# ============================================================
add_library(omniface_quality STATIC
    core/quality/openface/openface_quality_evaluator.cc
    core/quality/openface/openface_data_cleaner.cc
    core/quality/mediapipe/mediapipe_quality_evaluator.cc
    core/quality/mediapipe/mediapipe_data_cleaner.cc
    core/quality/l2cs/l2cs_quality_evaluator.cc
    core/quality/l2cs/l2cs_data_cleaner.cc
    core/quality/mefarg_au/mefarg_au_quality_evaluator.cc
    core/quality/mefarg_au/mefarg_au_data_cleaner.cc
    ${PROJECT_SOURCE_DIR}/include/omniface/core/quality/base_quality_evaluator.h
    ${PROJECT_SOURCE_DIR}/include/omniface/core/quality/quality_types.h
    ${PROJECT_SOURCE_DIR}/include/omniface/core/quality/base_data_cleaner.h
    ${PROJECT_SOURCE_DIR}/include/omniface/core/quality/openface/openface_quality_types.h
    ${PROJECT_SOURCE_DIR}/include/omniface/core/quality/openface/openface_quality_evaluator.h
    ${PROJECT_SOURCE_DIR}/include/omniface/core/quality/openface/openface_data_cleaner.h
    ${PROJECT_SOURCE_DIR}/include/omniface/core/quality/mediapipe/mediapipe_quality_types.h
    ${PROJECT_SOURCE_DIR}/include/omniface/core/quality/mediapipe/mediapipe_quality_evaluator.h
    ${PROJECT_SOURCE_DIR}/include/omniface/core/quality/mediapipe/mediapipe_data_cleaner.h
    ${PROJECT_SOURCE_DIR}/include/omniface/core/quality/l2cs/l2cs_quality_types.h
    ${PROJECT_SOURCE_DIR}/include/omniface/core/quality/l2cs/l2cs_quality_evaluator.h
    ${PROJECT_SOURCE_DIR}/include/omniface/core/quality/l2cs/l2cs_data_cleaner.h
    ${PROJECT_SOURCE_DIR}/include/omniface/core/quality/mefarg_au/mefarg_au_quality_types.h
    ${PROJECT_SOURCE_DIR}/include/omniface/core/quality/mefarg_au/mefarg_au_quality_evaluator.h
    ${PROJECT_SOURCE_DIR}/include/omniface/core/quality/mefarg_au/mefarg_au_data_cleaner.h
)

target_include_directories(omniface_quality PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(omniface_quality PUBLIC
    Qt6::Core
    spdlog::spdlog
)

set_target_properties(omniface_quality PROPERTIES AUTOMOC ON)

target_compile_features(omniface_quality PUBLIC cxx_std_20)

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
    pipeline/base_pipeline.cc
    pipeline/openface_pipeline.cc
    pipeline/mediapipe_pipeline.cc
    pipeline/l2cs_pipeline.cc
    pipeline/mefarg_au_pipeline.cc
    pipeline/recognition_pipeline.cc
    pipeline/pipeline_factory.cc
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

# ============================================================
# Database layer — SQLite via Qt6::Sql (no pipeline dependency)
# ============================================================
add_library(omniface_db STATIC
    db/snowflake.cpp
    db/database.cpp
    db/video_repo.cpp
    db/image_repo.cpp
    db/task_repo.cpp
)

target_include_directories(omniface_db PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(omniface_db PUBLIC
    Qt6::Sql
    spdlog::spdlog
)

set_target_properties(omniface_db PROPERTIES AUTOMOC OFF)

target_compile_features(omniface_db PUBLIC cxx_std_20)

# ============================================================
# UI layer — Qt Widgets + Pipeline + DB
# ============================================================
add_library(omniface_ui STATIC
    ui/main_window.cpp
    ui/theme.cc
    ui/pages/home_page.cpp
    ui/pages/video_management_page.cpp
    ui/pages/image_management_page.cpp
    ui/pages/processing_page.cpp
    ui/pages/task_history_page.cpp
    ui/pages/video_detail_page.cpp
    ui/pages/image_detail_page.cpp
    ui/pages/task_detail_page.cpp
    ui/dialogs/import_video_dialog.cpp
    ui/dialogs/import_image_dialog.cpp
    ui/dialogs/settings_dialog.cpp
    ui/dialogs/edit_video_dialog.cpp
    ui/dialogs/edit_image_dialog.cpp
    ui/dialogs/edit_task_dialog.cpp
    ui/dialogs/about_dialog.cpp
    ui/dialogs/video_selection_dialog.cpp
    ui/dialogs/image_selection_dialog.cpp
    ui/dialogs/csv_reference_dialog.cpp
    ui/widgets/calendar_widget.cpp
    ui/widgets/date_picker.cpp
    ui/widgets/video_player_widget.cpp
    ui/widgets/detail/openface_detail_widget.cpp
    ui/widgets/detail/mediapipe_detail_widget.cpp
    ui/widgets/detail/l2cs_detail_widget.cpp
    ui/widgets/detail/mefarg_au_detail_widget.cpp
    ui/widgets/detail/quality_report_widget.cpp
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/main_window.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/theme.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/pages/home_page.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/pages/video_management_page.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/pages/image_management_page.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/pages/processing_page.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/pages/task_history_page.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/pages/video_detail_page.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/pages/image_detail_page.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/pages/task_detail_page.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/dialogs/import_video_dialog.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/dialogs/import_image_dialog.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/dialogs/settings_dialog.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/dialogs/edit_video_dialog.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/dialogs/edit_image_dialog.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/dialogs/edit_task_dialog.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/dialogs/about_dialog.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/dialogs/video_selection_dialog.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/dialogs/image_selection_dialog.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/dialogs/csv_reference_dialog.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/widgets/calendar_widget.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/widgets/date_picker.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/widgets/video_player_widget.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/widgets/detail/base_detail_widget.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/widgets/detail/openface_detail_widget.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/widgets/detail/mediapipe_detail_widget.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/widgets/detail/l2cs_detail_widget.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/widgets/detail/mefarg_au_detail_widget.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/widgets/detail/quality_report_widget.h
)

target_include_directories(omniface_ui PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(omniface_ui PUBLIC
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Charts
    Qt6::Svg
    Qt6::SvgWidgets
    Qt6::Concurrent
    omniface_db
    omniface_pipeline
    omniface_quality
)

target_compile_features(omniface_ui PUBLIC cxx_std_20)

# ============================================================
# OmniFace executable
# ============================================================
qt_add_executable(OmniFace
    ui/main.cpp
    ${PROJECT_SOURCE_DIR}/resources/omniface.rc
    ${PROJECT_SOURCE_DIR}/resources/ui/resources.qrc
)

target_link_libraries(OmniFace PRIVATE
    omniface_ui
)

# Add Qt bin dir to PATH when running from Visual Studio
set_target_properties(OmniFace PROPERTIES
    VS_DEBUGGER_ENVIRONMENT "QT_MEDIA_BACKEND=${OMNIFACE_MEDIA_BACKEND};PATH=C:\\Qt\\6.11.1\\msvc2022_64\\bin;${CMAKE_SOURCE_DIR}/third_party/OpenFace/build/bin/$<CONFIG>;%PATH%"
)

# Post-build: deploy Qt DLLs alongside the executable
add_custom_command(TARGET OmniFace POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E echo "Running windeployqt..."
    COMMAND "C:/Qt/6.11.1/msvc2022_64/bin/windeployqt.exe" --no-translations "$<TARGET_FILE:OmniFace>"
    COMMENT "Deploying Qt runtime dependencies"
)

# Post-build: copy ONNX Runtime DLLs (GPU 1.23.2) to output directory
if(OMNIFACE_ONNXRUNTIME_ROOT)
    add_custom_command(TARGET OmniFace POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy
            "${OMNIFACE_ONNXRUNTIME_ROOT}/lib/onnxruntime.dll"
            "${OMNIFACE_ONNXRUNTIME_ROOT}/lib/onnxruntime_providers_shared.dll"
            "${OMNIFACE_ONNXRUNTIME_ROOT}/lib/onnxruntime_providers_cuda.dll"
            "${OMNIFACE_ONNXRUNTIME_ROOT}/lib/onnxruntime_providers_tensorrt.dll"
            "$<TARGET_FILE_DIR:OmniFace>"
        COMMENT "Copying ONNX Runtime 1.23.2 DLLs..."
    )
endif()

# Post-build: copy OpenFace model files
# NOTE: OpenFace is pre-built in Release config; if building Debug, update the
# source path or build OpenFace in the matching configuration first.
set(OFACE_BIN_DIR "${CMAKE_SOURCE_DIR}/third_party/OpenFace/build/bin/$<CONFIG>")
add_custom_command(TARGET OmniFace POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:OmniFace>/openface_model"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${OFACE_BIN_DIR}/model"
        "$<TARGET_FILE_DIR:OmniFace>/openface_model/model"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${OFACE_BIN_DIR}/classifiers"
        "$<TARGET_FILE_DIR:OmniFace>/openface_model/classifiers"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${OFACE_BIN_DIR}/AU_predictors"
        "$<TARGET_FILE_DIR:OmniFace>/openface_model/AU_predictors"
    COMMENT "Deploying OpenFace model files"
)

