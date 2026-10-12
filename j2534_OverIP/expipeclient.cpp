#include "ExPipeClient.h"
#include <future>
#include <chrono>

ExPipeClient::ExPipeClient(const std::string& name) : pipe(INVALID_HANDLE_VALUE), name(name)
{

}

ExPipeClient::~ExPipeClient() {
    if (pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe);
    }
}

bool ExPipeClient::connect() {
    const int maxAttempts = 3;
    const std::chrono::seconds sleepDuration(10);

    //auto future = std::async(std::launch::async, [=]() {
        for (int attempt = 0; attempt < maxAttempts; ++attempt) {
            printf("connect to named pipe attempt %d\n", attempt);
            pipe = CreateFileA(
                name.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0,
                NULL,
                OPEN_EXISTING,
                0,
                NULL
                );

            if (pipe != INVALID_HANDLE_VALUE) {
                // Successfully connected to the named pipe
                printf("Successfully connected to the named pipe\n");
                return true;
            }

            DWORD error = GetLastError();
            if (error == ERROR_FILE_NOT_FOUND)
            {
                // The named pipe does not exist, so start the server application
                // Read the path of the server application from the registry
                printf("The named pipe does not exist, so start the server application\n");
                HKEY hKey;
                if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\PassThruSupport.04.04\\XplatformsPassThruOverIP", 0, KEY_READ, &hKey) == ERROR_SUCCESS ||
                    RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\PassThruSupport.05.00\\XplatformsPassThruOverIP", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                    char path[MAX_PATH];
                    DWORD pathSize = sizeof(path);
                    if (RegQueryValueExA(hKey, "ConfigApplication", NULL, NULL, (LPBYTE)path, &pathSize) == ERROR_SUCCESS) {
                        // Start the server application
                        printf("Start the server application %s\n", path);
                        STARTUPINFOA si = { sizeof(si) };
                        PROCESS_INFORMATION pi;
                        if (!CreateProcessA(NULL, path, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                            // Failed to start the server application
                            printf("Failed to start the server application. Error: %d\n", GetLastError());
                            return false;
                        } else {
                            // Successfully started the server application
                            printf("Successfully started the server application %s\n", path);
                            CloseHandle(pi.hProcess);
                            CloseHandle(pi.hThread);
                        }
                    }
                    RegCloseKey(hKey);
                }
            } else if (error == ERROR_PIPE_BUSY) {
                // The pipe is busy, all instances are connected
                // Wait for a while and then try again
                printf("The named pipe is busy, waiting for a while and then try again\n");
            } else {
                // Some other error occurred
                printf("ExPipeClient CreateFile Failed! Error: %d\n", (int)error);
                return false;
            }

            // Sleep for a while and then try again
            std::this_thread::sleep_for(sleepDuration);
        }

        // Reached the maximum number of attempts without successfully connecting
        printf("ExPipeClient Failed to connect to named pipe after %d attempts\n", maxAttempts);
        return false;
   // });

    printf("future.get!");
    //return future.get();
}



void ExPipeClient::disconnect() {
    if (pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe);
        pipe = INVALID_HANDLE_VALUE;
    }
}

void ExPipeClient::send(PipeMessageBuilder& message)
{
    const auto encoded = message.encode();
    if (encoded.empty()) {
        printf("ExPipeClient: failed to encode pipe message\n");
        return;
    }
    writeFrame(encoded.data(), static_cast<uint32_t>(encoded.size()));
}

// Frame: little-endian int32 byte length, then the payload.
bool ExPipeClient::writeFrame(const uint8_t* data, uint32_t size)
{
    if (pipe == INVALID_HANDLE_VALUE)
        return false;

    DWORD bytesWritten = 0;
    const int32_t length = static_cast<int32_t>(size);
    if (!WriteFile(pipe, &length, sizeof(length), &bytesWritten, nullptr) || bytesWritten != sizeof(length)) {
        printf("ExPipeClient: can't write frame length to pipe\n");
        return false;
    }
    if (!WriteFile(pipe, data, size, &bytesWritten, nullptr) || bytesWritten != size) {
        printf("ExPipeClient: can't write frame to pipe\n");
        return false;
    }
    return true;
}
