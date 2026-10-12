# cn-cbor (jimsch fork), built from source at a pinned commit.
# Only the library sources are used; upstream's CMakeLists (CTest, Doxygen,
# coveralls, /WX) is skipped by pointing SOURCE_SUBDIR at a non-existent dir.
include(FetchContent)

FetchContent_Declare(cn-cbor
    GIT_REPOSITORY https://github.com/jimsch/cn-cbor.git
    GIT_TAG        f713bf67bcf3e076d47e474ce060252ef8be48c7
    SOURCE_SUBDIR  _no_cmake_)
FetchContent_MakeAvailable(cn-cbor)

add_library(cn-cbor STATIC
    ${cn-cbor_SOURCE_DIR}/src/cn-cbor.c
    ${cn-cbor_SOURCE_DIR}/src/cn-create.c
    ${cn-cbor_SOURCE_DIR}/src/cn-encoder.c
    ${cn-cbor_SOURCE_DIR}/src/cn-error.c
    ${cn-cbor_SOURCE_DIR}/src/cn-get.c
    ${cn-cbor_SOURCE_DIR}/src/cn-print.c)
target_include_directories(cn-cbor
    PUBLIC  ${cn-cbor_SOURCE_DIR}/include
    PRIVATE ${cn-cbor_SOURCE_DIR}/src)
target_compile_definitions(cn-cbor PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(cn-cbor PUBLIC ws2_32)
# Linked into the PassThru DLL, which uses the static CRT.
set_target_properties(cn-cbor PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
