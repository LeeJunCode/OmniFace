add_executable(omniface main.cc)
target_include_directories(omniface PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(omniface PRIVATE omniface_pipeline omniface_server omniface_viz)
target_compile_features(omniface PRIVATE cxx_std_20)
