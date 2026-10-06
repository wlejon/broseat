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
    broseat_test(test_dbus LIBS PkgConfig::SYSTEMD)
    # Both change the user manager's environment / units: one at a time.
    set_tests_properties(test_session test_autostart PROPERTIES RESOURCE_LOCK user_manager)
else()
    broseat_test(test_unavailable)
endif()
