add_library(omniface_ui STATIC
    main_window.cpp
    process_page.cpp
    detail_page.cpp
    history_page.cpp
    saved_media_page.cpp
    reference_face_service.cpp
    video_player_widget.cpp
    processing_dashboard.cpp
    au_panel.cpp
    onnx_analysis_panel.cpp
    settings_dialog.cpp
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/main_window.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/process_page.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/detail_page.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/history_page.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/saved_media_page.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/reference_face_service.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/app_state.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/stylesheet.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/video_player_widget.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/processing_dashboard.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/au_panel.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/image_utils.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/settings_dialog.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/onnx_analysis_panel.h
)

target_include_directories(omniface_ui PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(omniface_ui PUBLIC
    omniface_pipeline
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)

target_compile_features(omniface_ui PUBLIC cxx_std_20)

# Enable MOC, UIC, RCC for Qt
set_target_properties(omniface_ui PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
