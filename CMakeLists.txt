cmake_minimum_required(VERSION 3.24)

project(broseat VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(_broseat_top_level OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_broseat_top_level ON)
endif()

option(BROSEAT_BUILD_TESTS "Build broseat tests" ${_broseat_top_level})

function(broseat_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /MP)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)
pkg_check_modules(LIBSEAT REQUIRED IMPORTED_TARGET libseat)

file(GLOB_RECURSE BROSEAT_SOURCES CONFIGURE_DEPENDS
    src/common/*.cpp
    src/dbus/*.cpp
    src/seat/*.cpp
    src/session/*.cpp
    src/autostart/*.cpp
    src/inhibit/*.cpp
)

add_library(broseat STATIC ${BROSEAT_SOURCES})
add_library(broseat::broseat ALIAS broseat)

target_include_directories(broseat
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

broseat_warnings(broseat)

target_link_libraries(broseat
    PUBLIC
        Threads::Threads
        PkgConfig::SYSTEMD
        PkgConfig::LIBSEAT
)

if(BROSEAT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
