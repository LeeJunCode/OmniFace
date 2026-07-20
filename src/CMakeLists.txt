add_library(omniface_detail STATIC
    core/detail/onnx_session.cc
    core/detail/preprocess.cc)
target_include_directories(omniface_detail PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(omniface_detail PUBLIC onnxruntime::onnxruntime ${OpenCV_LIBS})
target_compile_features(omniface_detail PUBLIC cxx_std_20)

add_library(omniface_detection STATIC
    core/detection/face_detector.cc)
target_include_directories(omniface_detection PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(omniface_detection PUBLIC omniface_detail)
target_compile_features(omniface_detection PUBLIC cxx_std_20)

add_library(omniface_tracking STATIC
    core/tracking/kalman_filter.cc
    core/tracking/strack.cc
    core/tracking/lapjv.cc
    core/tracking/bytetrack_engine.cc)
target_include_directories(omniface_tracking PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(omniface_tracking PUBLIC omniface_detail Eigen3::Eigen)
target_compile_features(omniface_tracking PUBLIC cxx_std_20)

add_library(omniface_recognition STATIC
    core/recognition/face_recognizer.cc
    core/recognition/face_matcher.cc)
target_include_directories(omniface_recognition PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(omniface_recognition PUBLIC omniface_detail)
target_compile_features(omniface_recognition PUBLIC cxx_std_20)

add_library(omniface_pose STATIC
    core/analysis/pose_estimator.cc)
target_include_directories(omniface_pose PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(omniface_pose PUBLIC omniface_detail ${OpenCV_LIBS})
target_compile_features(omniface_pose PUBLIC cxx_std_20)

add_library(omniface_gaze STATIC
    core/analysis/gaze_estimator.cc)
target_include_directories(omniface_gaze PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(omniface_gaze PUBLIC omniface_detail)
target_compile_features(omniface_gaze PUBLIC cxx_std_20)

add_library(omniface_au STATIC
    core/analysis/au_analyzer.cc)
target_include_directories(omniface_au PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(omniface_au PUBLIC omniface_detail)
target_compile_features(omniface_au PUBLIC cxx_std_20)

add_library(omniface_openface STATIC
    core/analysis/openface_analyzer.cc
    core/analysis/dlib_stub.cc)
target_include_directories(omniface_openface PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(omniface_openface PUBLIC
    of_landmark of_face_analyser of_gaze of_utils)
target_compile_features(omniface_openface PUBLIC cxx_std_20)

add_library(omniface_viz STATIC
    viz/visualizer.cc)
target_include_directories(omniface_viz PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(omniface_viz PUBLIC ${OpenCV_LIBS})
target_compile_features(omniface_viz PUBLIC cxx_std_20)

add_library(omniface_pipeline STATIC
    pipeline/config.cc
    pipeline/pipeline.cc
    pipeline/pipeline_processing.cc)
target_include_directories(omniface_pipeline PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(omniface_pipeline PUBLIC
    omniface_detection omniface_tracking omniface_recognition
    omniface_pose omniface_gaze omniface_au omniface_openface
    omniface_viz yaml-cpp::yaml-cpp ${OpenCV_LIBS})
target_compile_features(omniface_pipeline PUBLIC cxx_std_20)

add_library(omniface_server STATIC
    server/web_server.cc
    server/web_server_pipeline.cc
    server/json_utils.cc)
target_include_directories(omniface_server PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(omniface_server PUBLIC omniface_pipeline httplib::httplib)
target_compile_features(omniface_server PUBLIC cxx_std_20)
