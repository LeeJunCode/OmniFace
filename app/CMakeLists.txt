find_program(WINDEPLOYQT_EXECUTABLE windeployqt
    HINTS "${Qt6_DIR}/../../../bin" "${Qt6_DIR}/../../bin"
    REQUIRED
)

add_executable(OmniFace
    main.cpp
    ${PROJECT_SOURCE_DIR}/resources/omniface.rc
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
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)

target_compile_features(OmniFace PRIVATE cxx_std_20)

set_target_properties(OmniFace PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    WIN32_EXECUTABLE TRUE
)

# 构建后：复制模型、配置、启动脚本，创建数据目录，部署 Qt DLL
add_custom_command(TARGET OmniFace POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PROJECT_SOURCE_DIR}/models $<TARGET_FILE_DIR:OmniFace>/models
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PROJECT_SOURCE_DIR}/third_party/OpenFace/lib/local/LandmarkDetector/model $<TARGET_FILE_DIR:OmniFace>/openface_model
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PROJECT_SOURCE_DIR}/third_party/OpenFace/lib/local/FaceAnalyser/AU_predictors $<TARGET_FILE_DIR:OmniFace>/AU_predictors
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PROJECT_SOURCE_DIR}/config.yaml $<TARGET_FILE_DIR:OmniFace>/config.yaml
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PROJECT_SOURCE_DIR}/scripts/OmniFace.bat $<TARGET_FILE_DIR:OmniFace>/OmniFace.bat
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:OmniFace>/data/output
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:OmniFace>/data/upload/video
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:OmniFace>/data/upload/img
    COMMAND
        ${WINDEPLOYQT_EXECUTABLE} $<TARGET_FILE:OmniFace>
)

# 安装规则：将 EXE 及所有运行时依赖安装到指定目录
install(TARGETS OmniFace DESTINATION .)

# 安装模型、配置与 OpenFace 资源
install(DIRECTORY ${PROJECT_SOURCE_DIR}/models/ DESTINATION models)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/third_party/OpenFace/lib/local/LandmarkDetector/model/
        DESTINATION openface_model)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/third_party/OpenFace/lib/local/FaceAnalyser/AU_predictors/
        DESTINATION AU_predictors)
install(FILES ${PROJECT_SOURCE_DIR}/config.yaml DESTINATION .)
install(FILES ${PROJECT_SOURCE_DIR}/scripts/OmniFace.bat DESTINATION .)

# 用 windeployqt 部署 Qt 运行时 DLL
install(CODE "
    execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
        \"\${CMAKE_INSTALL_PREFIX}/OmniFace.exe\")
    ")

# 创建运行时需要的数据目录结构
install(CODE "
    file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/data/output\")
    file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/data/upload/video\")
    file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/data/upload/img\")
    ")
