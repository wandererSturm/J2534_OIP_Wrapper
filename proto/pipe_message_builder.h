#ifndef PIPE_MESSAGE_BUILDER_H
#define PIPE_MESSAGE_BUILDER_H

// Builds a j2534.pipe.PipeMessage with nanopb and encodes it.
// Qt-free: linked into the PassThru DLL and into the x64 round-trip test.
//
//   PipeMessageBuilder b(j2534_pipe_Module_MODULE_DLL_WRAPPER, client_id, process_path, "PassThruConnect");
//   auto& call = b.select(j2534_pipe_PipeMessage_connect_tag, b.msg().payload.connect);
//   call.device_id = DeviceID;
//   ...
//   b.result(call, retval);
//   pipe->send(b.encode());
//
// Variable-length fields (strings, bytes, repeated) are nanopb callbacks. The
// builder keeps the callback state alive until encode(). Raw pointers passed to
// str()/passthru_msg()/msgs() must stay valid until encode() returns.

#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <pb_encode.h>

#include "j2534_pipe.pb.h"
#include "PassThruStruct.h"

class PipeMessageBuilder
{
public:
    struct DllFunction {
        std::string name;
        bool available;
    };

    PipeMessageBuilder(j2534_pipe_Module module, uint32_t client_id, std::string process_path, std::string text = {});

    PipeMessageBuilder(const PipeMessageBuilder&) = delete;
    PipeMessageBuilder& operator=(const PipeMessageBuilder&) = delete;

    j2534_pipe_PipeMessage& msg() { return m_msg; }

    // Selects the oneof payload and returns the zeroed member to fill.
    template <typename T>
    T& select(pb_size_t tag, T& member)
    {
        m_msg.which_payload = tag;
        member = T{};
        return member;
    }

    // Records what the call returned to the host (and why, if it failed).
    template <typename Call>
    void result(Call& call, long status, std::string error = {})
    {
        call.has_result = true;
        call.result.status = static_cast<int32_t>(status);
        if (!error.empty())
            call.result.error = str(std::move(error));
    }

    // Null-terminated string owned by the caller; nullptr encodes nothing.
    pb_callback_t str(const char* s);
    // Copied into the builder.
    pb_callback_t str(std::string s);

    // Fills `out` from `m` and sets `has`. Does nothing if m is nullptr.
    void passthru_msg(bool& has, j2534_pipe_PassThruMsg& out, const PASSTHRU_MSG* m);
    // Repeated PassThruMsg; nullptr or count 0 encodes nothing.
    pb_callback_t msgs(const PASSTHRU_MSG* m, unsigned long count);

    pb_callback_t dll_functions(std::vector<DllFunction> functions);
    pb_callback_t uint32_list(const unsigned long* values, unsigned long count);

    // Serialized PipeMessage; empty on encoding failure.
    std::vector<uint8_t> encode();

private:
    using FieldEncoder = std::function<bool(pb_ostream_t*, const pb_field_t*)>;

    pb_callback_t callback(FieldEncoder encoder);

    j2534_pipe_PipeMessage m_msg;
    std::deque<FieldEncoder> m_encoders;  // stable addresses for pb_callback_t::arg
};

#endif // PIPE_MESSAGE_BUILDER_H
