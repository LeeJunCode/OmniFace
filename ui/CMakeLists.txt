add_library(omniface_ui STATIC
    main_window.cpp
    video_preview_widget.cpp
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/main_window.h
    ${PROJECT_SOURCE_DIR}/include/omniface/ui/video_preview_widget.h
)

target_include_directories(omniface_ui PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(omniface_ui PUBLIC
    omniface_pipeline
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

target_compile_features(omniface_ui PUBLIC cxx_std_20)

# Enable MOC, UIC, RCC for Qt
set_target_properties(omniface_ui PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
