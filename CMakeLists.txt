cmake_minimum_required(VERSION 3.24)

project(broseat VERSION 0.1.0 LANGUAGES CXX)

# Dependencies (cmake/bro_deps.cmake): an outer project's target wins, else the
# ../<name> working tree beside the top-level project, else the pinned commit,
# fetched at configure.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/bro_deps.cmake)

# Targets
#   broseat::broseat   seat + session lifecycle for a Linux desktop session
#
# Sources:
#   src/common, src/autostart (parsing, conditions, Exec= expansion),
#   src/inhibit/{idle_aggregator,inhibitor_lock}.cpp, src/seat/seat_device.cpp
#                      portable, built everywhere
#   src/dbus, src/seat, src/session, src/inhibit, src/autostart/autostart_launcher.cpp
#                      Linux: sd-bus (logind, systemd user manager) and libseat
#   src/unavailable    Windows / macOS: the Linux entry points, reporting why
#                      they cannot work there

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(_broseat_top_level OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_broseat_top_level ON)
endif()

option(BROSEAT_BUILD_TESTS "Build the broseat tests" ${_broseat_top_level})

# Coverage. Scoped to a top-level build: a consumer that add_subdirectory()s
# broseat has no business having its own build instrumented from in here.
option(BROSEAT_COVERAGE "Instrument the build for gcov coverage (GCC/Clang)" OFF)
if(BROSEAT_COVERAGE AND _broseat_top_level)
    if(MSVC)
        message(FATAL_ERROR "BROSEAT_COVERAGE requires GCC or Clang (gcov instrumentation)")
    endif()
    add_compile_options(--coverage -fprofile-update=atomic -O0 -g)
    add_link_options(--coverage)
    message(STATUS "broseat: coverage instrumentation ON (-O0)")
endif()

# MSVC runtime: the default, the DLL CRT (/MD, /MDd), as bro builds with. The
# api's tests load bronze_runtime_shared.dll and pass std::string and vectors
# across that boundary; with the static CRT each module has its own heap, and
# memory freed on the other side is a Debug assert (__acrt_first_block ==
# header) and silent heap corruption in Release.
if(MSVC AND _broseat_top_level)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

function(broseat_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /MP)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

find_package(Threads REQUIRED)

set(BROSEAT_SOURCES
    src/common/types.cpp
    src/common/utils.cpp
    src/autostart/autostart_condition.cpp
    src/autostart/autostart_desktop.cpp
    src/autostart/autostart_discovery.cpp
    src/autostart/autostart_exec.cpp
    src/autostart/autostart_manager.cpp
    src/inhibit/idle_aggregator.cpp
    src/inhibit/inhibitor_lock.cpp
    src/seat/seat_device.cpp)

set(BROSEAT_LINUX OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(BROSEAT_LINUX ON)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)
    pkg_check_modules(LIBSEAT REQUIRED IMPORTED_TARGET libseat)

    # --- brodbus (dbus library) ---
    bro_dependency(brodbus GITHUB wlejon/brodbus REF 714ded627aed1c0db4b046d143bcc29e1ec9e9cf OPTIONS BRODBUS_BUILD_TESTS=OFF)

    list(APPEND BROSEAT_SOURCES
        src/dbus/bus.cpp
        src/seat/libseat_backend.cpp
        src/seat/logind_backend.cpp
        src/seat/seat_manager.cpp
        src/session/environment.cpp
        src/session/session_manager.cpp
        src/session/unit_manager.cpp
        src/inhibit/inhibit_manager.cpp
        src/autostart/autostart_launcher.cpp)
else()
    message(STATUS "broseat: not Linux; seat, session, inhibit and launch entry points report themselves unavailable")
    list(APPEND BROSEAT_SOURCES src/unavailable/unavailable.cpp)
endif()

add_library(broseat STATIC ${BROSEAT_SOURCES})
add_library(broseat::broseat ALIAS broseat)

target_include_directories(broseat
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

broseat_warnings(broseat)

target_link_libraries(broseat PUBLIC Threads::Threads)
if(BROSEAT_LINUX)
    target_link_libraries(broseat PUBLIC brodbus::brodbus PRIVATE PkgConfig::SYSTEMD PkgConfig::LIBSEAT)
endif()

# --- Standalone Bronze JavaScript API ---
# On by default only when broseat is the top-level project. A library that
# consumes broseat then never needs bronze checked out; bro, which does want
# the binding, sets BROSEAT_ENABLE_API ON itself.
option(BROSEAT_ENABLE_API "Build Bronze JavaScript API" ${PROJECT_IS_TOP_LEVEL})
if(BROSEAT_ENABLE_API)
    # bronze (with brass, its code generator) through cmake/bro_deps.cmake: an
    # outer project's target, else ../bronze beside the top-level project, else
    # the pinned commit, fetched at configure.
    bro_dependency(bronze GITHUB wlejon/bronze REF 15ce4cf5fc25d8afe03882985ae95adcba46a6a9 TARGET bronze_runtime_shared
                   OPTIONS BRONZE_BUILD_SHARED_RUNTIME=ON BRONZE_BUILD_TESTS=OFF)

    add_subdirectory(src/api)
endif()

if(BROSEAT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
