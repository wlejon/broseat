cmake_minimum_required(VERSION 3.24)

project(broseat VERSION 0.1.0 LANGUAGES CXX)

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

if(MSVC AND _broseat_top_level)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
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
    if(NOT TARGET brodbus::brodbus)
        set(BRODBUS_DIR "${CMAKE_SOURCE_DIR}/../brodbus" CACHE PATH
            "Path to standalone brodbus repo (set to a missing path to force submodule fallback)")
        if(EXISTS "${BRODBUS_DIR}/CMakeLists.txt")
            message(STATUS "broseat: brodbus from ${BRODBUS_DIR}")
            add_subdirectory("${BRODBUS_DIR}" "${CMAKE_BINARY_DIR}/brodbus" EXCLUDE_FROM_ALL)
        elseif(EXISTS "${CMAKE_SOURCE_DIR}/third_party/brodbus/CMakeLists.txt")
            message(STATUS "broseat: brodbus from ${CMAKE_SOURCE_DIR}/third_party/brodbus (submodule)")
            add_subdirectory("${CMAKE_SOURCE_DIR}/third_party/brodbus" "${CMAKE_BINARY_DIR}/brodbus" EXCLUDE_FROM_ALL)
        else()
            message(FATAL_ERROR
                "broseat needs brodbus (dbus library): not found at ${BRODBUS_DIR} or "
                "${CMAKE_SOURCE_DIR}/third_party/brodbus. Clone https://github.com/wlejon/brodbus next "
                "to this repo, run `git submodule update --init --recursive`, or override with "
                "-DBRODBUS_DIR=...")
        endif()
    endif()

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
    if(NOT TARGET bronze_runtime_shared)
        set(BRONZE_BUILD_SHARED_RUNTIME ON CACHE BOOL "" FORCE)
        set(BRONZE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        if(DEFINED BRONZE_DIR AND EXISTS "${BRONZE_DIR}/CMakeLists.txt")
            # Explicit BRONZE_DIR supplied
        elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../bronze/CMakeLists.txt")
            set(BRONZE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../bronze" CACHE PATH "bronze source tree")
        elseif(EXISTS "${CMAKE_SOURCE_DIR}/../bronze/CMakeLists.txt")
            set(BRONZE_DIR "${CMAKE_SOURCE_DIR}/../bronze" CACHE PATH "bronze source tree")
        endif()

        if(NOT BRONZE_DIR OR NOT EXISTS "${BRONZE_DIR}/CMakeLists.txt")
            message(FATAL_ERROR
                "bronze not found: check it out beside this repository (../bronze, with ../brass beside it) "
                "or pass -DBRONZE_DIR=<path>.")
        endif()

        add_subdirectory("${BRONZE_DIR}" "${CMAKE_BINARY_DIR}/bronze-build" EXCLUDE_FROM_ALL)
    endif()

    add_subdirectory(src/api)
endif()

if(BROSEAT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
