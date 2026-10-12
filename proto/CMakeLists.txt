set(J2534_PIPE_PROTO "${CMAKE_CURRENT_SOURCE_DIR}/j2534_pipe.proto")

# ---------------------------------------------------------------------------
# nanopb side (Qt-free): PassThru DLL and the round-trip test

if(J2534_BUILD_DLL OR J2534_BUILD_TESTS)
    set(_nanopb_out "${CMAKE_CURRENT_BINARY_DIR}/nanopb")
    nanopb_generate(_nanopb_generated "${J2534_PIPE_PROTO}" "${_nanopb_out}")
    # One generation step shared by every builder variant below.
    add_custom_target(j2534_pipe_nanopb_generate DEPENDS ${_nanopb_generated})

    # j2534_add_pipe_builder(<target> <msvc runtime>)
    # The DLL needs the static CRT; the Qt test needs the dynamic one.
    function(j2534_add_pipe_builder target runtime)
        add_library(${target} STATIC
            pipe_message_builder.cpp
            pipe_message_builder.h
            ${_nanopb_generated}
            ${NANOPB_SOURCE_DIR}/pb_common.c
            ${NANOPB_SOURCE_DIR}/pb_encode.c)
        add_dependencies(${target} j2534_pipe_nanopb_generate)
        target_include_directories(${target} PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${_nanopb_out}
            ${NANOPB_SOURCE_DIR}
            ${PROJECT_SOURCE_DIR}/j2534_OverIP)   # PassThruStruct.h
        set_target_properties(${target} PROPERTIES MSVC_RUNTIME_LIBRARY "${runtime}")
    endfunction()

    if(J2534_BUILD_DLL)
        j2534_add_pipe_builder(j2534_pipe_builder_static_crt "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
    if(J2534_BUILD_TESTS)
        j2534_add_pipe_builder(j2534_pipe_builder "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
endif()

# ---------------------------------------------------------------------------
# QtProtobuf side: App and the round-trip test

if(J2534_BUILD_APP OR J2534_BUILD_TESTS)
    qt_add_library(j2534_pipe_qt STATIC)
    qt_add_protobuf(j2534_pipe_qt
        PROTO_FILES ${J2534_PIPE_PROTO}
        OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qt")
    target_link_libraries(j2534_pipe_qt PUBLIC Qt6::Protobuf)
endif()
