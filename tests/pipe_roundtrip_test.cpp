// Encodes pipe messages with the DLL's nanopb builder and decodes them with
// QtProtobuf, as the App does. Guards the wire contract between the two.

#include <QTest>
#include <QProtobufSerializer>
#include <QProtobufJsonSerializer>

#include <cstring>
#include <vector>

#include <QTemporaryDir>
#include <QFile>

#include "pipe_message_builder.h"
#include "j2534_pipe.qpb.h"
#include "pipecapture.h"

using namespace j2534::pipe;
using Payload = PipeMessage::PayloadFields;

namespace {

constexpr uint32_t kClientId = 0xCAFEBABE;
const char* kProcessPath = "C:\\Program Files (x86)\\Host\\host.exe";

PipeMessage decode(const std::vector<uint8_t>& bytes)
{
    QProtobufSerializer serializer;
    PipeMessage msg;
    const bool ok = msg.deserialize(&serializer,
        QByteArrayView(reinterpret_cast<const char*>(bytes.data()), qsizetype(bytes.size())));
    if (!ok)
        qWarning() << "deserialize failed:" << serializer.lastErrorString();
    return msg;
}

PASSTHRU_MSG make_msg(unsigned long protocol, unsigned long size, unsigned char fill)
{
    PASSTHRU_MSG m{};
    m.ProtocolID = protocol;
    m.RxStatus = 0x100;
    m.TxFlags = 0x40;
    m.Timestamp = 123456;
    m.DataSize = size;
    m.ExtraDataIndex = 7;
    std::memset(m.Data, fill, std::min<size_t>(size, sizeof(m.Data)));
    return m;
}

} // namespace

class PipeRoundTripTest : public QObject
{
    Q_OBJECT

private slots:
    void envelopeOnly()
    {
        PipeMessageBuilder b(j2534_pipe_Module_MODULE_EMULATOR, kClientId, kProcessPath, "PassThruIoctl called!");
        const auto msg = decode(b.encode());

        QCOMPARE(msg.version(), VersionGadget::Version::VERSION_CURRENT);
        QCOMPARE(quint32(msg.clientId()), kClientId);
        QCOMPARE(msg.module(), ModuleGadget::Module::MODULE_EMULATOR);
        QVERIFY(msg.timestampMs() > 1'700'000'000'000);   // ms since epoch, not seconds
        QCOMPARE(msg.processPath(), QString::fromLatin1(kProcessPath));
        QCOMPARE(msg.text(), QStringLiteral("PassThruIoctl called!"));
        QCOMPARE(msg.payloadField(), Payload::UninitializedField);
    }

    void connectCall()
    {
        PipeMessageBuilder b(j2534_pipe_Module_MODULE_DLL_WRAPPER, kClientId, kProcessPath);
        auto& call = b.select(j2534_pipe_PipeMessage_connect_tag, b.msg().payload.connect);
        call.device_id = 1;
        call.protocol_id = 6;           // ISO15765
        call.flags = 0x800;
        call.baudrate = 500000;
        call.channel_id = 3;
        b.result(call, 0);

        const auto msg = decode(b.encode());
        QCOMPARE(msg.payloadField(), Payload::Connect);
        const auto& c = msg.connect();
        QCOMPARE(quint32(c.deviceId()), 1u);
        QCOMPARE(quint32(c.protocolId()), 6u);
        QCOMPARE(quint32(c.flags()), 0x800u);
        QCOMPARE(quint32(c.baudrate()), 500000u);
        QCOMPARE(quint32(c.channelId()), 3u);
        QVERIFY(c.hasResult());
        QCOMPARE(qint32(c.result().status()), 0);
        QVERIFY(c.result().error().isEmpty());
    }

    void readMsgsWithBatch()
    {
        PASSTHRU_MSG msgs[3] = {
            make_msg(6, 3, 0x30),
            make_msg(5, sizeof(PASSTHRU_MSG::Data), 0xAA),  // full 4128-byte payload
            make_msg(5, 9000, 0x55),                        // driver reports more than fits
        };

        PipeMessageBuilder b(j2534_pipe_Module_MODULE_CANNELLONI, kClientId, kProcessPath);
        auto& call = b.select(j2534_pipe_PipeMessage_read_msgs_tag, b.msg().payload.read_msgs);
        call.channel_id = 3;
        call.num_msgs_requested = 10;
        call.timeout = 250;
        call.msgs = b.msgs(msgs, 3);
        b.result(call, 0x10, "ERR_BUFFER_EMPTY");

        const auto msg = decode(b.encode());
        QCOMPARE(msg.payloadField(), Payload::ReadMsgs);
        const auto& r = msg.readMsgs();
        QCOMPARE(quint32(r.channelId()), 3u);
        QCOMPARE(quint32(r.numMsgsRequested()), 10u);
        QCOMPARE(quint32(r.timeout()), 250u);
        QCOMPARE(qint32(r.result().status()), 0x10);
        QCOMPARE(r.result().error(), QStringLiteral("ERR_BUFFER_EMPTY"));

        QCOMPARE(r.msgs().size(), 3);
        const auto& first = r.msgs().at(0);
        QCOMPARE(quint32(first.protocolId()), 6u);
        QCOMPARE(quint32(first.rxStatus()), 0x100u);
        QCOMPARE(quint32(first.txFlags()), 0x40u);
        QCOMPARE(quint32(first.timestamp()), 123456u);
        QCOMPARE(quint32(first.dataSize()), 3u);
        QCOMPARE(quint32(first.extraDataIndex()), 7u);
        QCOMPARE(first.data(), QByteArray(3, char(0x30)));

        QCOMPARE(r.msgs().at(1).data(), QByteArray(4128, char(0xAA)));

        const auto& clamped = r.msgs().at(2);
        QCOMPARE(quint32(clamped.dataSize()), 9000u);              // what the driver said
        QCOMPARE(clamped.data(), QByteArray(4128, char(0x55)));   // what was actually there
    }

    void readMsgsWithoutMessages()
    {
        PipeMessageBuilder b(j2534_pipe_Module_MODULE_DLL_WRAPPER, kClientId, kProcessPath);
        auto& call = b.select(j2534_pipe_PipeMessage_read_msgs_tag, b.msg().payload.read_msgs);
        call.channel_id = 1;
        call.msgs = b.msgs(nullptr, 5);
        b.result(call, 0x07, "Function PassThruReadMsgs not found in DLL.");

        const auto msg = decode(b.encode());
        QCOMPARE(msg.payloadField(), Payload::ReadMsgs);
        QVERIFY(msg.readMsgs().msgs().isEmpty());
        QCOMPARE(msg.readMsgs().result().error(), QStringLiteral("Function PassThruReadMsgs not found in DLL."));
    }

    void startMsgFilterWithoutFlowControl()
    {
        PASSTHRU_MSG mask = make_msg(6, 4, 0xFF);
        PASSTHRU_MSG pattern = make_msg(6, 4, 0x07);

        PipeMessageBuilder b(j2534_pipe_Module_MODULE_DLL_WRAPPER, kClientId, kProcessPath);
        auto& call = b.select(j2534_pipe_PipeMessage_start_msg_filter_tag, b.msg().payload.start_msg_filter);
        call.channel_id = 2;
        call.filter_type = 1;   // PASS_FILTER
        b.passthru_msg(call.has_mask, call.mask, &mask);
        b.passthru_msg(call.has_pattern, call.pattern, &pattern);
        b.passthru_msg(call.has_flow_control, call.flow_control, nullptr);
        call.filter_id = 42;
        b.result(call, 0);

        const auto msg = decode(b.encode());
        const auto& f = msg.startMsgFilter();
        QVERIFY(f.hasMask());
        QVERIFY(f.hasPattern());
        QVERIFY(!f.hasFlowControl());
        QCOMPARE(f.mask().data(), QByteArray(4, char(0xFF)));
        QCOMPARE(f.pattern().data(), QByteArray(4, char(0x07)));
        QCOMPARE(quint32(f.filterId()), 42u);
    }

    void readVersionStrings()
    {
        char firmware[80] = "04.04";
        char dll[80] = "1.0.001 Feb 28 2023 12:46:00";

        PipeMessageBuilder b(j2534_pipe_Module_MODULE_EMULATOR, kClientId, kProcessPath);
        auto& call = b.select(j2534_pipe_PipeMessage_read_version_tag, b.msg().payload.read_version);
        call.device_id = 777;
        call.firmware_version = b.str(firmware);
        call.dll_version = b.str(dll);
        call.api_version = b.str(static_cast<const char*>(nullptr));
        b.result(call, 0);

        const auto msg = decode(b.encode());
        const auto& v = msg.readVersion();
        QCOMPARE(v.firmwareVersion(), QStringLiteral("04.04"));
        QCOMPARE(v.dllVersion(), QStringLiteral("1.0.001 Feb 28 2023 12:46:00"));
        QVERIFY(v.apiVersion().isEmpty());
    }

    void dllLoadedFunctions()
    {
        PipeMessageBuilder b(j2534_pipe_Module_MODULE_DLL_WRAPPER, kClientId, kProcessPath, "wrapped functions");
        auto& loaded = b.select(j2534_pipe_PipeMessage_dll_loaded_tag, b.msg().payload.dll_loaded);
        loaded.path = b.str(std::string("C:\\Drivers\\op20pt32.dll"));
        loaded.functions = b.dll_functions({{"PassThruOpen", true}, {"PassThruExReadFlash", false}});

        const auto msg = decode(b.encode());
        QCOMPARE(msg.payloadField(), Payload::DllLoaded);
        const auto& d = msg.dllLoaded();
        QCOMPARE(d.path(), QStringLiteral("C:\\Drivers\\op20pt32.dll"));
        QCOMPARE(d.functions().size(), 2);
        QCOMPARE(d.functions().at(0).name(), QStringLiteral("PassThruOpen"));
        QVERIFY(d.functions().at(0).available());
        QCOMPARE(d.functions().at(1).name(), QStringLiteral("PassThruExReadFlash"));
        QVERIFY(!d.functions().at(1).available());
    }

    void selectChannelIds()
    {
        unsigned long channels[] = {1, 300, 70000};

        PipeMessageBuilder b(j2534_pipe_Module_MODULE_DLL_WRAPPER, kClientId, kProcessPath);
        auto& call = b.select(j2534_pipe_PipeMessage_select_tag, b.msg().payload.select);
        call.channel_ids = b.uint32_list(channels, 3);
        call.channel_threshold = 1;
        call.select_type = 1;
        call.timeout = 100;

        const auto msg = decode(b.encode());
        const auto& s = msg.select();
        QCOMPARE(s.channelIds().size(), 3);
        QCOMPARE(quint32(s.channelIds().at(0)), 1u);
        QCOMPARE(quint32(s.channelIds().at(1)), 300u);
        QCOMPARE(quint32(s.channelIds().at(2)), 70000u);
    }

    void vendorCall()
    {
        PipeMessageBuilder b(j2534_pipe_Module_MODULE_DLL_WRAPPER, kClientId, kProcessPath);
        auto& call = b.select(j2534_pipe_PipeMessage_vendor_tag, b.msg().payload.vendor);
        call.vendor_function = j2534_pipe_VendorFunction_VENDOR_FUNCTION_EX_ERASE_FLASH;
        b.result(call, 0x07);

        const auto msg = decode(b.encode());
        QCOMPARE(msg.payloadField(), Payload::Vendor);
        QCOMPARE(msg.vendor().vendorFunction(), VendorFunctionGadget::VendorFunction::VENDOR_FUNCTION_EX_ERASE_FLASH);
        QCOMPARE(qint32(msg.vendor().result().status()), 0x07);
    }

    // export.json stores messages with QProtobufJsonSerializer; make sure they load back.
    void jsonRoundTrip()
    {
        PASSTHRU_MSG m = make_msg(6, 5, 0x11);
        PipeMessageBuilder b(j2534_pipe_Module_MODULE_CANNELLONI, kClientId, kProcessPath, "PassThruWriteMsgs");
        auto& call = b.select(j2534_pipe_PipeMessage_write_msgs_tag, b.msg().payload.write_msgs);
        call.channel_id = 2;
        call.msgs = b.msgs(&m, 1);
        call.num_msgs_written = 1;
        b.result(call, 0);

        const auto original = decode(b.encode());

        QProtobufJsonSerializer json;
        const QByteArray text = original.serialize(&json);
        QVERIFY2(text.contains("writeMsgs"), text.constData());

        PipeMessage reloaded;
        QVERIFY2(reloaded.deserialize(&json, text), qPrintable(json.lastErrorString()));
        QCOMPARE(reloaded.payloadField(), Payload::WriteMsgs);
        QCOMPARE(reloaded.writeMsgs().msgs().at(0).data(), QByteArray(5, char(0x11)));
        QCOMPARE(reloaded.processPath(), original.processPath());
    }

    void captureFileRoundTrip()
    {
        PASSTHRU_MSG m = make_msg(6, 4, 0x22);
        PipeMessageBuilder b(j2534_pipe_Module_MODULE_DLL_WRAPPER, kClientId, kProcessPath, "PassThruReadMsgs");
        auto& call = b.select(j2534_pipe_PipeMessage_read_msgs_tag, b.msg().payload.read_msgs);
        call.msgs = b.msgs(&m, 1);
        b.result(call, 0);

        QList<PipeMessage> saved{decode(b.encode()),
                                 decode(PipeMessageBuilder(j2534_pipe_Module_MODULE_EMULATOR, kClientId, kProcessPath, "log line").encode())};

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("export.json");

        QString error;
        QVERIFY2(PipeCapture::save(path, saved, &error), qPrintable(error));

        QList<PipeMessage> loaded;
        QVERIFY2(PipeCapture::load(path, &loaded, &error), qPrintable(error));
        QCOMPARE(loaded.size(), 2);
        QCOMPARE(loaded.at(0).readMsgs().msgs().at(0).data(), QByteArray(4, char(0x22)));
        QCOMPARE(loaded.at(1).text(), QStringLiteral("log line"));
        QCOMPARE(loaded.at(1).module(), ModuleGadget::Module::MODULE_EMULATOR);
    }

    void captureFileRejectsCborFormat()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("old_export.json");
        QFile file(path);
        QVERIFY(file.open(QFile::WriteOnly));
        // Shape written by the CBOR-based App: a bare array with integer-keyed data maps.
        file.write(R"([{"id": 1, "type": 2, "data_type": 2, "timestamp": 0, "path": "x", "message": "PassThruOpen", "data": {"0": 20}}])");
        file.close();

        QList<PipeMessage> loaded;
        QString error;
        QVERIFY(!PipeCapture::load(path, &loaded, &error));
        QVERIFY(loaded.isEmpty());
        QVERIFY2(error.contains("older version"), qPrintable(error));
    }
};

QTEST_GUILESS_MAIN(PipeRoundTripTest)
#include "pipe_roundtrip_test.moc"
