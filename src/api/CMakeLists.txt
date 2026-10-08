add_library(broseat_api STATIC
    api.cpp
    host_class.cpp
    native_session.cpp
    native_inhibit.cpp
    native_autostart.cpp
    native_idle.cpp
)

target_include_directories(broseat_api PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
)

target_link_libraries(broseat_api PUBLIC
    broseat
    bronze_runtime_shared
)

target_compile_features(broseat_api PUBLIC cxx_std_20)

broseat_warnings(broseat_api)
