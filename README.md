# J2534_OIP_Wrapper

## Overview
J2534_OIP_Wrapper is a multi-functional tool designed for J2534 driver emulation and Over-IP communication. It allows automotive developers to remotely interface with J2534 devices, simulating, logging, and interrupting the communication for testing and diagnostic purposes. The wrapper will be compatible with PassThru 4.04 and 5.05 versions, ensuring broad support for various J2534 applications and standards.

> **Note:** This project is currently under heavy development. Features, APIs, and functionalities are subject to change as improvements and updates are being actively made.

## Key Features
- **Over-IP Communication:** Enables remote interaction with J2534 devices over IP networks.
- **Emulation & Simulation:** Emulates J2534 drivers and simulates their behavior for diagnostics and testing.
- **Logging & Interrupting:** Captures and logs data while also providing the ability to interrupt communication at specific points for testing.
- **Remote Diagnostics & Firmware Updates:** Supports automotive diagnostics, control, and firmware flashing over networks.
- **PassThru 4.04 & 5.05 Support:** Full compatibility with J2534 PassThru specifications 4.04 and 5.05, making it versatile across different automotive standards.
- **Automatic Seed/Key Handling:** Future support will include automatic seed/key generation processes, leveraging algorithms and code developed in related projects:
  - [mbseedkey](https://github.com/Xplatforms/mbseedkey)
  - [SeedKeyAlgosSandbox](https://github.com/Xplatforms/SeedKeyAlgosSandbox)
  - [CPC_NG_11_0B_Xplatforms_OpenSource](https://github.com/Xplatforms/CPC_NG_11_0B_Xplatforms_OpenSource)
  - [CFFFlashFileTools](https://github.com/Xplatforms/CFFFlashFileTools)

## Technologies
- Written in C++ and QML
- J2534 API support for versatile automotive applications

## Building
Requirements: Windows, Visual Studio 2022 (MSVC), CMake 3.25+, Qt 6.8+ (msvc2022_64) with the Qt Protobuf module for the App, Python 3 with the `protobuf` package, and `protoc` from vcpkg (`vcpkg install protobuf:x64-windows`, with `VCPKG_ROOT` set).

The project builds two artifacts with separate presets:

| Preset | Output | Notes |
|--------|--------|-------|
| `x86`  | `j2534_OverIP.dll` (32-bit PassThru driver), `j2534_device_host.exe` | No Qt, static CRT; the DLL depends only on `ws2_32`, `advapi32`, `kernel32`, `winhttp` |
| `x64`  | `j2534_OverIP_App.exe` (64-bit Qt App), `cloudflared.exe` | Needs `QTDIR` pointing at your Qt kit; downloads a pinned cloudflared (SHA-256 checked, `-DJ2534_BUNDLE_CLOUDFLARED=OFF` to use the one on PATH) |

```sh
# PassThru DLL (x86)
cmake --preset x86
cmake --build --preset x86
cmake --install build/x86 --config Release    # -> dist/x86/j2534_OverIP.dll

# Qt App (x64)
set QTDIR=C:/Qt/6.11.0/msvc2022_64
cmake --preset x64
cmake --build --preset x64
cmake --install build/x64 --config Release    # -> dist/x64/bin/j2534_OverIP_App.exe + Qt runtime
```

Qt Creator can open the top-level `CMakeLists.txt` directly.

The DLL and the App exchange the messages defined in `proto/j2534_pipe.proto`: the DLL encodes them with
[nanopb](https://github.com/nanopb/nanopb) (no Qt), the App decodes them with Qt Protobuf. The x64 preset also
builds a round-trip test that checks both sides agree:

```sh
ctest --preset x64
```

### Registering the driver
Start `dist\x64\bin\j2534_OverIP_App.exe` and pick an interface (Cannelloni, DLL Wrapper or Emulator). The App asks
for administrator rights once and writes `HKLM\SOFTWARE\WOW6432Node\PassThruSupport.04.04\XplatformsPassThruOverIP`:

- `ExSelectedInterface`: what the DLL does (`J2534Cannelloni`, `J2534DllWrapper`, `J2534Emulator`, `J2534Remote`); `WrappedDll` for the wrapper
- `FunctionLibrary`: full path to `dist\x86\j2534_OverIP.dll`, so host applications list the driver
- `ConfigApplication`: full path to the App, which the DLL starts if it isn't already running
- `Name`, `Vendor` and the `CAN`, `ISO15765`, `ISO9141` protocol flags

Change the interface later with **Change…** on the Wrapper/Cannelloni settings pages or at the top of the Emulator
view. Restart the diagnostic program afterwards: the DLL reads these values when it is loaded. The page also shows
the equivalent `reg add` commands for manual setup.

## Remote adapter: Vehicle PC and Diagnostic PC
The adapter and the diagnostic program can be on different PCs anywhere on the internet. Neither needs a static IP
address or open ports: the connection goes through a [trycloudflare](https://try.cloudflare.com) quick tunnel.

| | **Vehicle PC** (orange) | **Diagnostic PC** (blue) |
|---|---|---|
| Where | At the car, with the J2534 adapter | Runs the diagnostic program |
| In the App | Interface page → **Share this PC's adapter** | Interface card **Remote adapter (Diagnostic PC)** |
| What runs | The App, `cloudflared`, `j2534_device_host.exe` (loads the 32-bit adapter driver) | `j2534_OverIP.dll` with `ExSelectedInterface = J2534Remote` |

1. On the Vehicle PC, choose the adapter's driver and click **Start sharing**. The App opens the tunnel and shows a
   connection code such as `brave-cat-xyz#K7P4-9QXM-2M8D` (tunnel name and a per-session secret) with a **Copy** button.
2. On the Diagnostic PC, paste the code into the Remote adapter card. The code is stored per user in
   `HKCU\Software\Xplatforms\J2534OverIP\RemoteConnection`, so changing it needs no administrator rights.
3. Start the diagnostic program and open the device *J2534 Over IP (Xplatforms)*. The Vehicle PC shows who is asking;
   nothing reaches the adapter until someone clicks **Allow** there.

Only one Diagnostic PC can connect at a time. Three wrong codes stop sharing, and every start makes a new code.
When the link drops, the Vehicle PC disconnects the channels and closes the adapter, which also stops periodic messages.

Quick tunnels are meant for testing: they come with no uptime guarantee, and the name changes every time sharing starts.

Without the UI, `j2534_OverIP_App --vehicle-server --driver <driver.dll> [--tunnel]` shares an adapter from the
command line and prints the code. The x86 tests include an end-to-end run over localhost; set `J2534_E2E_TUNNEL=1` to
send it through a real tunnel instead.

## License
This project is licensed under GPL-3.0.

## Contributions
Feedback and contributions are welcome! Please keep in mind that the project is in active development, so expect ongoing changes and improvements.
