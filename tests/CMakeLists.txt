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

qt_add_executable(passthru_registry_test
    passthru_registry_test.cpp
    ${PROJECT_SOURCE_DIR}/j2534_OverIP_App/passthruregistry.cpp)
target_include_directories(passthru_registry_test PRIVATE ${PROJECT_SOURCE_DIR}/j2534_OverIP_App)
set_target_properties(passthru_registry_test PROPERTIES WIN32_EXECUTABLE FALSE)
target_link_libraries(passthru_registry_test PRIVATE Qt6::Core Qt6::Test)

add_test(NAME passthru_registry COMMAND passthru_registry_test)
set_tests_properties(passthru_registry PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;QT_FORCE_STDERR_LOGGING=set:1")

# VehicleServer's Allow/Reject flow and the quick tunnel parser. The test exe
# is also its own fake device host (see runFakeHost()).
qt_add_executable(vehicle_server_test
    vehicle_server_test.cpp
    ${PROJECT_SOURCE_DIR}/j2534_OverIP_App/vehicleserver.cpp
    ${PROJECT_SOURCE_DIR}/j2534_OverIP_App/cloudflaredtunnel.cpp)
target_include_directories(vehicle_server_test PRIVATE ${PROJECT_SOURCE_DIR}/j2534_OverIP_App)
set_target_properties(vehicle_server_test PROPERTIES WIN32_EXECUTABLE FALSE)
target_link_libraries(vehicle_server_test PRIVATE
    j2534_pipe_qt
    Qt6::Network Qt6::WebSockets Qt6::Protobuf Qt6::Test)

add_test(NAME vehicle_server COMMAND vehicle_server_test)
set_tests_properties(vehicle_server PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;QT_FORCE_STDERR_LOGGING=set:1")
