# broseat_test(<name> [TIMEOUT s] [LIBS ...]): one real ctest from <name>.cpp.
# Exit code 77 is a skip (a service or privilege genuinely absent, reason
# printed), anything else nonzero a failure.
function(broseat_test name)
    cmake_parse_arguments(T "" "TIMEOUT" "LIBS" ${ARGN})
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE broseat::broseat ${T_LIBS})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src)
    broseat_warnings(${name})
    if(NOT T_TIMEOUT)
        set(T_TIMEOUT 60)
    endif()
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT ${T_TIMEOUT} SKIP_RETURN_CODE 77)
endfunction()

# Portable: every platform.
broseat_test(test_types)
broseat_test(test_event_queue)
broseat_test(test_autostart)
broseat_test(test_idle)

if(BROSEAT_LINUX)
    # Against the machine's own logind, systemd user manager and seat.
    broseat_test(test_seat)
    broseat_test(test_session)
    broseat_test(test_inhibit)
    broseat_test(test_dbus LIBS brodbus::brodbus PkgConfig::SYSTEMD)
    # Both change the user manager's environment / units: one at a time.
    set_tests_properties(test_session test_autostart PROPERTIES RESOURCE_LOCK user_manager)
else()
    broseat_test(test_unavailable)
endif()

if(BROSEAT_ENABLE_API)
    add_executable(broseat_test_api
        test_seat_api.cpp
    )
    target_include_directories(broseat_test_api PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/api
        ${PROJECT_SOURCE_DIR}/src
    )
    target_link_libraries(broseat_test_api PRIVATE
        broseat_api
        broseat
        bronze_runtime_shared
    )
    broseat_warnings(broseat_test_api)
    if(WIN32)
        target_compile_definitions(broseat_test_api PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE _WIN32_WINNT=0x0A00)
        add_custom_command(TARGET broseat_test_api POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:bronze_runtime_shared>
                $<TARGET_FILE_DIR:broseat_test_api>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_LINKER_FILE:bronze_runtime_shared>
                $<TARGET_FILE_DIR:broseat_test_api>
        )
    endif()

    if(NOT BRASS_HOST_BACKEND)
        message(STATUS "broseat: broseat_test_api not registered (brass has no code generator for this target)")
    else()
        add_test(NAME broseat_test_api COMMAND broseat_test_api)
        set_tests_properties(broseat_test_api PROPERTIES TIMEOUT 60 SKIP_RETURN_CODE 77)

        add_test(NAME broseat_test_api_gc_stress COMMAND broseat_test_api)
        set_tests_properties(broseat_test_api_gc_stress PROPERTIES
            TIMEOUT 60
            SKIP_RETURN_CODE 77
            ENVIRONMENT "BRONZE_GC_STRESS=1;BRONZE_GC_POISON=1"
        )
        if(WIN32)
            set_tests_properties(broseat_test_api PROPERTIES
                ENVIRONMENT "PATH=$<TARGET_FILE_DIR:bronze_runtime_shared>;$<TARGET_FILE_DIR:broseat_test_api>;$ENV{PATH}")
            set_property(TEST broseat_test_api_gc_stress APPEND PROPERTY
                ENVIRONMENT "PATH=$<TARGET_FILE_DIR:bronze_runtime_shared>;$<TARGET_FILE_DIR:broseat_test_api>;$ENV{PATH}")
        endif()
    endif()
endif()
