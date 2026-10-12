# protoc from the vcpkg protobuf port. It is a host tool, so the x64 build in
# vcpkg serves both the x86 DLL and the x64 App presets.
# Override with -DPROTOC_EXECUTABLE=<path> when vcpkg lives elsewhere.
find_program(PROTOC_EXECUTABLE
    NAMES protoc
    HINTS "$ENV{VCPKG_ROOT}/installed/x64-windows/tools/protobuf"
    NO_DEFAULT_PATH)

if(NOT PROTOC_EXECUTABLE)
    message(FATAL_ERROR
        "protoc not found. Install it with `vcpkg install protobuf:x64-windows` and set "
        "VCPKG_ROOT, or pass -DPROTOC_EXECUTABLE=<path to protoc.exe>.")
endif()

execute_process(
    COMMAND "${PROTOC_EXECUTABLE}" --version
    OUTPUT_VARIABLE _protoc_version_output
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _protoc_result)
if(NOT _protoc_result EQUAL 0)
    message(FATAL_ERROR "Failed to run ${PROTOC_EXECUTABLE} --version")
endif()
string(REGEX MATCH "[0-9]+(\\.[0-9]+)*" PROTOC_VERSION "${_protoc_version_output}")
message(STATUS "Using protoc ${PROTOC_VERSION}: ${PROTOC_EXECUTABLE}")

# Qt's FindWrapProtoc returns early when this target exists, so qt_add_protobuf
# uses the same protoc instead of searching for a protobuf CMake package.
if(NOT TARGET WrapProtoc::WrapProtoc)
    add_executable(WrapProtoc::WrapProtoc IMPORTED GLOBAL)
    set_target_properties(WrapProtoc::WrapProtoc PROPERTIES
        IMPORTED_LOCATION "${PROTOC_EXECUTABLE}"
        _qt_internal_protobuf_version "${PROTOC_VERSION}")
endif()
