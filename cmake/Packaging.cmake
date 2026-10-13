# One Qt Installer Framework setup (CPack IFW) for both architectures:
#
#   App           x64/bin: the App, Qt and VC++ runtime (always installed)
#   DiagnosticPC  x86/j2534_OverIP.dll, registered for diagnostic programs
#   VehiclePC     x86/j2534_device_host.exe, x64/bin/cloudflared.exe
#
# Both builds install into one tree: this one with J2534_ARCH_DIR=x64, the
# x86 build tree (J2534_X86_BUILD_DIR) with J2534_ARCH_DIR=x86, the same
# layout as dist/. The "release" preset builds the x86 tree itself
# (J2534_BUILD_X86_SUBPROJECT).

option(J2534_BUILD_X86_SUBPROJECT "Also build the x86 DLL and device host (for packaging)" OFF)
set(J2534_X86_BUILD_DIR "" CACHE PATH "x86 build tree to package (default: the x86 subproject)")

if(J2534_BUILD_X86_SUBPROJECT)
    include(ExternalProject)
    set(_x86_dir "${CMAKE_BINARY_DIR}/x86")
    ExternalProject_Add(j2534_x86
        SOURCE_DIR "${PROJECT_SOURCE_DIR}"
        BINARY_DIR "${_x86_dir}"
        CMAKE_GENERATOR "${CMAKE_GENERATOR}"
        CMAKE_GENERATOR_PLATFORM Win32
        CMAKE_ARGS -DJ2534_BUILD_DLL=ON -DJ2534_BUILD_APP=OFF -DJ2534_BUILD_TESTS=OFF -DJ2534_ARCH_DIR=x86
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config $<CONFIG>
        INSTALL_COMMAND ""
        BUILD_ALWAYS ON
        USES_TERMINAL_BUILD ON)
    if(NOT J2534_X86_BUILD_DIR)
        set(J2534_X86_BUILD_DIR "${_x86_dir}")
    endif()
endif()

find_program(J2534_IFW_BINARYCREATOR binarycreator
    HINTS "$ENV{QTIFWDIR}/bin" "C:/Qt/Tools/QtInstallerFramework/4.11/bin"
    DOC "Qt Installer Framework binarycreator")
if(J2534_IFW_BINARYCREATOR)
    get_filename_component(_ifw_bin "${J2534_IFW_BINARYCREATOR}" DIRECTORY)
    get_filename_component(CPACK_IFW_ROOT "${_ifw_bin}" DIRECTORY)
else()
    message(STATUS "Qt Installer Framework not found (set QTIFWDIR); packaging is unavailable")
endif()

set(CPACK_GENERATOR IFW)
set(CPACK_PACKAGE_NAME "J2534OverIP")
set(CPACK_PACKAGE_VENDOR "Xplatforms")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "J2534 PassThru driver emulation, wrapper and remote adapter")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}.0")
set(CPACK_PACKAGE_FILE_NAME "J2534OverIP-${PROJECT_VERSION}-setup")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")

set(CPACK_IFW_PACKAGE_TITLE "J2534 Over IP")
set(CPACK_IFW_PACKAGE_PUBLISHER "Xplatforms")
set(CPACK_IFW_PACKAGE_WIZARD_STYLE "Modern")
set(CPACK_IFW_TARGET_DIRECTORY "@ApplicationsDirX64@/J2534OverIP")
set(CPACK_IFW_PACKAGE_START_MENU_DIRECTORY "J2534 Over IP")
set(CPACK_IFW_PACKAGE_MAINTENANCE_TOOL_NAME "J2534OverIP-maintenance")
set(CPACK_IFW_PACKAGE_RUN_PROGRAM "@TargetDir@/x64/bin/j2534_OverIP_App.exe")
set(CPACK_IFW_PACKAGE_RUN_PROGRAM_DESCRIPTION "Start the J2534 Over IP App")
set(CPACK_IFW_PACKAGE_WINDOW_ICON "${PROJECT_SOURCE_DIR}/j2534_OverIP_App/assets/garage.png")

# Both build trees. (The 4th field would prefix the IFW staging directory,
# not the files, so the x64/x86 split comes from J2534_ARCH_DIR instead.)
if(J2534_X86_BUILD_DIR AND J2534_ARCH_DIR STREQUAL ".")
    message(WARNING "Packaging the x86 tree needs J2534_ARCH_DIR=x64 (use the \"release\" preset)")
endif()
set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};${PROJECT_NAME};ALL;/")
if(J2534_X86_BUILD_DIR)
    list(APPEND CPACK_INSTALL_CMAKE_PROJECTS "${J2534_X86_BUILD_DIR};${PROJECT_NAME};ALL;/")
else()
    message(STATUS "J2534_X86_BUILD_DIR not set: the package won't contain the x86 DLL and device host")
endif()
set(CPACK_COMPONENTS_ALL App DiagnosticPC VehiclePC)

include(CPack)
include(CPackIFW)

set(_scripts "${PROJECT_SOURCE_DIR}/cmake/installer")

cpack_add_component(App
    DISPLAY_NAME "J2534 Over IP App"
    DESCRIPTION "Chooses what the PassThru driver does, shows its traffic and shares adapters. Always installed."
    REQUIRED)
cpack_ifw_configure_component(App
    FORCED_INSTALLATION
    SORTING_PRIORITY 30
    SCRIPT "${_scripts}/app.qs")

cpack_add_component(DiagnosticPC
    DISPLAY_NAME "Diagnostic PC: PassThru driver"
    DESCRIPTION "The 32-bit J2534 driver that diagnostic programs load (Cannelloni, DLL Wrapper, Emulator or a "
                "Remote adapter on a Vehicle PC). Registered for diagnostic programs during setup."
    DEPENDS App)
cpack_ifw_configure_component(DiagnosticPC
    DEFAULT TRUE
    SORTING_PRIORITY 20
    SCRIPT "${_scripts}/diagnostic_pc.qs")

set(_vehicle_licenses "")
if(J2534_CLOUDFLARED_LICENSE)
    set(_vehicle_licenses LICENSES "cloudflared (Apache License 2.0)" "${J2534_CLOUDFLARED_LICENSE}")
endif()
cpack_add_component(VehiclePC
    DISPLAY_NAME "Vehicle PC: share an adapter"
    DESCRIPTION "Shares the J2534 adapter connected to this PC with a Diagnostic PC over the internet "
                "(device host and Cloudflare's cloudflared)."
    DEPENDS App)
cpack_ifw_configure_component(VehiclePC
    DEFAULT TRUE
    SORTING_PRIORITY 10
    ${_vehicle_licenses})
