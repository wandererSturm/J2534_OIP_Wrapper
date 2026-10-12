#ifndef EXPIPECLIENT_H
#define EXPIPECLIENT_H

#include <windows.h>
#include <string>
#include <vector>
#include "pipe_message_builder.h"

// Sends j2534.pipe.PipeMessage frames (see proto/j2534_pipe.proto) to the App.
class ExPipeClient {
public:
    explicit ExPipeClient(const std::string& name);
    ~ExPipeClient();

    // Deleted copy constructor and assignment operator
    ExPipeClient(const ExPipeClient&) = delete;
    ExPipeClient& operator=(const ExPipeClient&) = delete;

    bool connect();
    void disconnect();
    // Encodes and writes one length-prefixed j2534.pipe.PipeMessage.
    void send(PipeMessageBuilder& message);

private:
    bool writeFrame(const uint8_t* data, uint32_t size);

    HANDLE pipe;
    std::string name;
};

#endif // EXPIPECLIENT_H
