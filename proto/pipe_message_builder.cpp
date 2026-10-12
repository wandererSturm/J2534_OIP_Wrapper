#include "pipe_message_builder.h"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace {

bool encode_with(pb_ostream_t* stream, const pb_field_t* field, void* const* arg)
{
    auto encoder = static_cast<const std::function<bool(pb_ostream_t*, const pb_field_t*)>*>(*arg);
    return (*encoder)(stream, field);
}

bool encode_bytes(pb_ostream_t* stream, const pb_field_t* field, const void* data, size_t size)
{
    if (size == 0)
        return true;
    return pb_encode_tag_for_field(stream, field)
        && pb_encode_string(stream, static_cast<const pb_byte_t*>(data), size);
}

size_t clamped_data_size(const PASSTHRU_MSG& m)
{
    return std::min<size_t>(m.DataSize, sizeof(m.Data));
}

int64_t now_ms()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

} // namespace

PipeMessageBuilder::PipeMessageBuilder(j2534_pipe_Module module, uint32_t client_id, std::string process_path, std::string text)
    : m_msg{}
{
    m_msg.version = j2534_pipe_Version_VERSION_CURRENT;
    m_msg.client_id = client_id;
    m_msg.module = module;
    m_msg.timestamp_ms = now_ms();
    m_msg.process_path = str(std::move(process_path));
    m_msg.text = str(std::move(text));
}

pb_callback_t PipeMessageBuilder::callback(FieldEncoder encoder)
{
    m_encoders.push_back(std::move(encoder));
    pb_callback_t cb{};
    cb.funcs.encode = &encode_with;
    cb.arg = &m_encoders.back();
    return cb;
}

pb_callback_t PipeMessageBuilder::str(const char* s)
{
    return callback([s](pb_ostream_t* stream, const pb_field_t* field) {
        return s == nullptr || encode_bytes(stream, field, s, std::strlen(s));
    });
}

pb_callback_t PipeMessageBuilder::str(std::string s)
{
    return callback([s = std::move(s)](pb_ostream_t* stream, const pb_field_t* field) {
        return encode_bytes(stream, field, s.data(), s.size());
    });
}

void PipeMessageBuilder::passthru_msg(bool& has, j2534_pipe_PassThruMsg& out, const PASSTHRU_MSG* m)
{
    if (m == nullptr)
        return;

    has = true;
    out = j2534_pipe_PassThruMsg_init_zero;
    out.protocol_id = m->ProtocolID;
    out.rx_status = m->RxStatus;
    out.tx_flags = m->TxFlags;
    out.timestamp = m->Timestamp;
    out.data_size = m->DataSize;
    out.extra_data_index = m->ExtraDataIndex;
    out.data = callback([m](pb_ostream_t* stream, const pb_field_t* field) {
        return encode_bytes(stream, field, m->Data, clamped_data_size(*m));
    });
}

pb_callback_t PipeMessageBuilder::msgs(const PASSTHRU_MSG* m, unsigned long count)
{
    return callback([m, count](pb_ostream_t* stream, const pb_field_t* field) {
        if (m == nullptr)
            return true;
        for (unsigned long i = 0; i < count; ++i) {
            const PASSTHRU_MSG& src = m[i];
            j2534_pipe_PassThruMsg out = j2534_pipe_PassThruMsg_init_zero;
            out.protocol_id = src.ProtocolID;
            out.rx_status = src.RxStatus;
            out.tx_flags = src.TxFlags;
            out.timestamp = src.Timestamp;
            out.data_size = src.DataSize;
            out.extra_data_index = src.ExtraDataIndex;

            // Local encoder: lives only for this submessage.
            std::function<bool(pb_ostream_t*, const pb_field_t*)> data_encoder =
                [&src](pb_ostream_t* s, const pb_field_t* f) {
                    return encode_bytes(s, f, src.Data, clamped_data_size(src));
                };
            out.data.funcs.encode = &encode_with;
            out.data.arg = &data_encoder;

            if (!pb_encode_tag_for_field(stream, field)
                || !pb_encode_submessage(stream, j2534_pipe_PassThruMsg_fields, &out))
                return false;
        }
        return true;
    });
}

pb_callback_t PipeMessageBuilder::dll_functions(std::vector<DllFunction> functions)
{
    return callback([functions = std::move(functions)](pb_ostream_t* stream, const pb_field_t* field) {
        for (const auto& fn : functions) {
            j2534_pipe_DllLoaded_Function out = j2534_pipe_DllLoaded_Function_init_zero;
            out.available = fn.available;

            std::function<bool(pb_ostream_t*, const pb_field_t*)> name_encoder =
                [&fn](pb_ostream_t* s, const pb_field_t* f) {
                    return encode_bytes(s, f, fn.name.data(), fn.name.size());
                };
            out.name.funcs.encode = &encode_with;
            out.name.arg = &name_encoder;

            if (!pb_encode_tag_for_field(stream, field)
                || !pb_encode_submessage(stream, j2534_pipe_DllLoaded_Function_fields, &out))
                return false;
        }
        return true;
    });
}

pb_callback_t PipeMessageBuilder::uint32_list(const unsigned long* values, unsigned long count)
{
    return callback([values, count](pb_ostream_t* stream, const pb_field_t* field) {
        if (values == nullptr || count == 0)
            return true;

        // proto3 packs repeated scalars: tag, byte length, then the varints.
        pb_ostream_t sizing = PB_OSTREAM_SIZING;
        for (unsigned long i = 0; i < count; ++i)
            pb_encode_varint(&sizing, values[i]);

        if (!pb_encode_tag(stream, PB_WT_STRING, field->tag)
            || !pb_encode_varint(stream, sizing.bytes_written))
            return false;
        for (unsigned long i = 0; i < count; ++i) {
            if (!pb_encode_varint(stream, values[i]))
                return false;
        }
        return true;
    });
}

std::vector<uint8_t> PipeMessageBuilder::encode()
{
    size_t size = 0;
    if (!pb_get_encoded_size(&size, j2534_pipe_PipeMessage_fields, &m_msg))
        return {};

    std::vector<uint8_t> buffer(size);
    pb_ostream_t stream = pb_ostream_from_buffer(buffer.data(), buffer.size());
    if (!pb_encode(&stream, j2534_pipe_PipeMessage_fields, &m_msg))
        return {};

    buffer.resize(stream.bytes_written);
    return buffer;
}
