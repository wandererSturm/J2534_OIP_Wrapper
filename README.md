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
Requirements: Windows, Visual Studio 2022 (MSVC), CMake 3.21+, and Qt 6.5+ (msvc2022_64) for the App.

The project builds two artifacts with separate presets:

| Preset | Output | Notes |
|--------|--------|-------|
| `x86`  | `j2534_OverIP.dll` (32-bit PassThru driver) | No Qt, static CRT; dependencies are only `ws2_32`, `advapi32`, `kernel32` |
| `x64`  | `j2534_OverIP_App.exe` (64-bit Qt App) | Needs `QTDIR` pointing at your Qt kit |

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

### Registering the driver
Registry setup is manual for now. Host applications find the DLL, and the DLL finds the App, through
`HKLM\SOFTWARE\WOW6432Node\PassThruSupport.04.04\XplatformsPassThruOverIP` (or `PassThruSupport.05.00`):

- `FunctionLibrary`: full path to `dist\x86\j2534_OverIP.dll`
- `ConfigApplication`: full path to `dist\x64\bin\j2534_OverIP_App.exe`, which the DLL starts if the App isn't already running

## License
This project is licensed under GPL-3.0.

## Contributions
Feedback and contributions are welcome! Please keep in mind that the project is in active development, so expect ongoing changes and improvements.
