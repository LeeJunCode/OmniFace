add_library(omniface_ui STATIC
    main_window.cpp
    video_player_widget.cpp
    processing_dashboard.cpp
    au_panel.cpp
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/main_window.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/video_player_widget.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/processing_dashboard.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/au_panel.h
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
