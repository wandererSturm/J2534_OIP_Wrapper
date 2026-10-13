# cloudflared for the Vehicle PC: opens the trycloudflare quick tunnel to the
# App's local WebSocket server. A pinned release is downloaded once at
# configure time and checked against its published SHA-256.
#
#   J2534_CLOUDFLARED_EXE      the downloaded cloudflared.exe ("" if disabled)
#   J2534_CLOUDFLARED_LICENSE  its Apache-2.0 license

option(J2534_BUNDLE_CLOUDFLARED "Download cloudflared and install it with the App" ON)

set(J2534_CLOUDFLARED_VERSION "2026.9.3")
set(J2534_CLOUDFLARED_SHA256 "f096265ec2fcbe9bb6e2d64268db167ced3fcbb83d894bdb9e2fcdb26f2ea7e2")
set(J2534_CLOUDFLARED_LICENSE_SHA256 "58d1e17ffe5109a7ae296caafcadfdbe6a7d176f0bc4ab01e12a689b0499d8bd")

set(J2534_CLOUDFLARED_EXE "")
set(J2534_CLOUDFLARED_LICENSE "")

if(J2534_BUNDLE_CLOUDFLARED)
    set(_dir "${CMAKE_BINARY_DIR}/_deps/cloudflared-${J2534_CLOUDFLARED_VERSION}")
    set(_exe "${_dir}/cloudflared.exe")
    set(_license "${_dir}/LICENSE.cloudflared.txt")
    set(_base "https://github.com/cloudflare/cloudflared")

    # file(DOWNLOAD) re-checks the hash of an existing file and only
    # downloads again when it doesn't match.
    file(DOWNLOAD "${_base}/releases/download/${J2534_CLOUDFLARED_VERSION}/cloudflared-windows-amd64.exe" "${_exe}"
         EXPECTED_HASH SHA256=${J2534_CLOUDFLARED_SHA256}
         SHOW_PROGRESS STATUS _status)
    list(GET _status 0 _code)
    if(NOT _code EQUAL 0)
        list(GET _status 1 _message)
        message(FATAL_ERROR "Downloading cloudflared ${J2534_CLOUDFLARED_VERSION} failed: ${_message}\n"
                            "Configure with -DJ2534_BUNDLE_CLOUDFLARED=OFF to use cloudflared from PATH instead.")
    endif()

    file(DOWNLOAD "https://raw.githubusercontent.com/cloudflare/cloudflared/${J2534_CLOUDFLARED_VERSION}/LICENSE" "${_license}"
         EXPECTED_HASH SHA256=${J2534_CLOUDFLARED_LICENSE_SHA256}
         STATUS _status)
    list(GET _status 0 _code)
    if(NOT _code EQUAL 0)
        list(GET _status 1 _message)
        message(FATAL_ERROR "Downloading the cloudflared license failed: ${_message}")
    endif()

    set(J2534_CLOUDFLARED_EXE "${_exe}")
    set(J2534_CLOUDFLARED_LICENSE "${_license}")
    message(STATUS "cloudflared ${J2534_CLOUDFLARED_VERSION}: ${_exe}")
endif()
