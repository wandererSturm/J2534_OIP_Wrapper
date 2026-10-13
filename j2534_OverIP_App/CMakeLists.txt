

qt_add_executable(j2534_OverIP_App WIN32
    main.cpp)

qt_add_qml_module(j2534_OverIP_App
    URI de.xplatforms.j2534
    VERSION 1.0
    QML_FILES
        Main.qml
        MainPage.qml
        InterfacePage.qml
        InterfaceRow.qml
        RoleBanner.qml
        VehiclePage.qml
        RemotePage.qml
        ExIconButton.qml
        J2534Emulator.qml
        J2534Wrapper.qml
        WrapperMainView.qml
        WrapperSettings.qml
        WrapperRXView.qml
        WrapperTXView.qml
        WrapperRXTXQueuesView.qml
        J2534Cannelloni.qml
        CannelloniMainView.qml
        CannelloniSettings.qml
        CannelloniRXView.qml
        CannelloniRXTXQueuesView.qml
        CannelloniTXView.qml
    SOURCES
        defs.h
        types.h
        exj2534cannelloni.cpp exj2534cannelloni.h
        exj2534emulator.cpp exj2534emulator.h
        exj2534readmsgslist.cpp exj2534readmsgslist.h
        exj2534wrapper.cpp exj2534wrapper.h
        exj2534writemsgslist.cpp exj2534writemsgslist.h
        exmessagepreprocessor.cpp exmessagepreprocessor.h
        expipeserver.cpp expipeserver.h
        msgstableviewmodel.cpp msgstableviewmodel.h
        pipecapture.cpp pipecapture.h
        passthruregistry.cpp passthruregistry.h
        interfaceconfig.cpp interfaceconfig.h
        vehicleserver.cpp vehicleserver.h
        cloudflaredtunnel.cpp cloudflaredtunnel.h
        vehiclesharing.cpp vehiclesharing.h
        # Same connection code parser as the DLL, to check pasted codes.
        ${PROJECT_SOURCE_DIR}/j2534_remote_client/connection_code.cpp
        ${PROJECT_SOURCE_DIR}/j2534_remote_client/connection_code.h)

# Icons are referenced as qrc:/assets/... from QML, so keep them at the root prefix.
qt_add_resources(j2534_OverIP_App "assets"
    PREFIX /
    FILES
        assets/arrow_left.png
        assets/arrow_left.svg
        assets/arrow_right.png
        assets/arrow_right.svg
        assets/arrow2_left.png
        assets/arrow2_right.png
        assets/arrows.png
        assets/arrows.svg
        assets/garage.png
        assets/garage.svg
        assets/settings_icon.png)

# Only PassThruStruct.h is shared with the DLL; the App does not link it.
target_include_directories(j2534_OverIP_App PRIVATE
    ${PROJECT_SOURCE_DIR}/j2534_OverIP
    ${PROJECT_SOURCE_DIR}/j2534_remote_client)
target_compile_definitions(j2534_OverIP_App PRIVATE QT_DISABLE_DEPRECATED_BEFORE=0x060000)
target_link_libraries(j2534_OverIP_App PRIVATE
    j2534_pipe_qt
    shell32
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Widgets Qt6::Network Qt6::WebSockets)

include(GNUInstallDirs)
set(_bin_dir "${J2534_ARCH_DIR}/${CMAKE_INSTALL_BINDIR}")
install(TARGETS j2534_OverIP_App
    BUNDLE DESTINATION ${J2534_ARCH_DIR} COMPONENT App
    RUNTIME DESTINATION ${_bin_dir} COMPONENT App)

# VC++ runtime next to the App, so the installer needs no redistributable.
# (The x86 DLL and device host use the static CRT.)
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${_bin_dir})
set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT App)
include(InstallRequiredSystemLibraries)

# cloudflared next to the App (Vehicle PC), also in the build tree so the
# App works when started from the IDE.
include(Cloudflared)
if(J2534_CLOUDFLARED_EXE)
    add_custom_command(TARGET j2534_OverIP_App POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${J2534_CLOUDFLARED_EXE}" "$<TARGET_FILE_DIR:j2534_OverIP_App>/cloudflared.exe"
        VERBATIM)
    install(PROGRAMS "${J2534_CLOUDFLARED_EXE}" DESTINATION ${_bin_dir} COMPONENT VehiclePC)
    install(FILES "${J2534_CLOUDFLARED_LICENSE}" DESTINATION ${_bin_dir} COMPONENT VehiclePC)
endif()

qt_generate_deploy_qml_app_script(
    TARGET j2534_OverIP_App
    OUTPUT_SCRIPT deploy_script
    NO_TRANSLATIONS)
# Qt runtime into the same architecture directory as the App. (Qt's deploy
# support only sets these when they are empty.)
set(_qt_deploy_dirs "")
foreach(kind BIN LIB PLUGINS QML TRANSLATIONS)
    string(TOLOWER "${kind}" dir)
    if(kind STREQUAL "BIN")
        set(dir "${CMAKE_INSTALL_BINDIR}")
    endif()
    string(APPEND _qt_deploy_dirs "set(QT_DEPLOY_${kind}_DIR \"${J2534_ARCH_DIR}/${dir}\")\n")
endforeach()
install(CODE "${_qt_deploy_dirs}" COMPONENT App)
install(SCRIPT ${deploy_script} COMPONENT App)
