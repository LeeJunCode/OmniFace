find_program(WINDEPLOYQT_EXECUTABLE windeployqt
    HINTS "${Qt6_DIR}/../../../bin" "${Qt6_DIR}/../../bin"
    REQUIRED
)

add_executable(OmniFace
    main.cpp
)

target_include_directories(OmniFace PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(OmniFace PRIVATE
    omniface_pipeline
    omniface_ui
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

target_compile_features(OmniFace PRIVATE cxx_std_20)

set_target_properties(OmniFace PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    WIN32_EXECUTABLE TRUE
)

# Copy models, config and deploy Qt DLLs to build output
add_custom_command(TARGET OmniFace POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PROJECT_SOURCE_DIR}/models $<TARGET_FILE_DIR:OmniFace>/models
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PROJECT_SOURCE_DIR}/config.yaml $<TARGET_FILE_DIR:OmniFace>/config.yaml
    COMMAND
        ${WINDEPLOYQT_EXECUTABLE} $<TARGET_FILE:OmniFace>
)

install(TARGETS OmniFace DESTINATION .)
