qt_add_executable(pipe_roundtrip_test
    pipe_roundtrip_test.cpp
    ${PROJECT_SOURCE_DIR}/j2534_OverIP_App/pipecapture.cpp)
target_include_directories(pipe_roundtrip_test PRIVATE ${PROJECT_SOURCE_DIR}/j2534_OverIP_App)
# Console app so QtTest output reaches ctest (qt_standard_project_setup defaults to WIN32).
set_target_properties(pipe_roundtrip_test PROPERTIES WIN32_EXECUTABLE FALSE)
target_link_libraries(pipe_roundtrip_test PRIVATE
    j2534_pipe_builder
    j2534_pipe_qt
    Qt6::Protobuf
    Qt6::Test)

add_test(NAME pipe_roundtrip COMMAND pipe_roundtrip_test)
# Qt DLLs are not deployed next to the test, so put the kit on PATH. Without a
# console, Qt sends QtTest's log to the debugger instead of stderr.
set_tests_properties(pipe_roundtrip PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;QT_FORCE_STDERR_LOGGING=set:1")
