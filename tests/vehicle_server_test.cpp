// Vehicle PC pieces of the App that don't need the internet: the quick tunnel
// address parser and VehicleServer's Allow/Reject flow.
//
// The test exe doubles as a fake j2534_device_host.exe: started with
// `--pipe <name> --driver <dll>`, it connects to the pipe, reports "ready"
// and waits until the App closes the pipe.

#include <QLocalSocket>
#include <QProtobufSerializer>
#include <QSignalSpy>
#include <QTest>
#include <QWebSocket>
#include <QtEndian>

#include <cstring>

#include "cloudflaredtunnel.h"
#include "j2534_remote.qpb.h"
#include "vehicleserver.h"

using namespace j2534::remote;

namespace {

const QString kSecret = QStringLiteral("K7P4-9QXM-2M8D");

int runFakeHost(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QLocalSocket pipe;
    pipe.connectToServer(QString::fromLocal8Bit(argv[2]));
    if (!pipe.waitForConnected(5000))
        return 4;

    Frame ready;
    ready.setResponse(Response{});
    QProtobufSerializer serializer;
    const QByteArray bytes = ready.serialize(&serializer);
    QByteArray prefix(4, '\0');
    qToLittleEndian<quint32>(static_cast<quint32>(bytes.size()), prefix.data());
    pipe.write(prefix + bytes);
    pipe.waitForBytesWritten(5000);

    while (pipe.state() == QLocalSocket::ConnectedState)
        pipe.waitForReadyRead(1000), pipe.readAll();
    return 0;
}

QByteArray helloFrame(const QString &secret, const QString &name)
{
    Hello hello;
    hello.setVersion(VersionGadget::Version::VERSION_CURRENT);
    hello.setSecret(secret);
    hello.setClientName(name);
    Frame frame;
    frame.setHello(hello);
    QProtobufSerializer serializer;
    return frame.serialize(&serializer);
}

// A Diagnostic PC stand-in: connects, sends Hello, collects HelloResults.
struct Client
{
    QWebSocket socket;
    QList<HelloResult> results;

    explicit Client(quint16 port)
    {
        QObject::connect(&socket, &QWebSocket::binaryMessageReceived, [this](const QByteArray &data) {
            QProtobufSerializer serializer;
            Frame frame;
            if (frame.deserialize(&serializer, data) && frame.kindField() == Frame::KindFields::HelloResult)
                results.append(frame.helloResult());
        });
        socket.open(QUrl(QStringLiteral("ws://127.0.0.1:%1/").arg(port)));
    }

    bool hello(const QString &secret, const QString &name)
    {
        if (socket.state() != QAbstractSocket::ConnectedState
            && !QTest::qWaitFor([this] { return socket.state() == QAbstractSocket::ConnectedState; }, 5000))
            return false;
        socket.sendBinaryMessage(helloFrame(secret, name));
        return true;
    }

    bool waitForResult() { return QTest::qWaitFor([this] { return !results.isEmpty(); }, 10000); }
    HelloResult::Status status() const { return results.isEmpty() ? HelloResult::Status::STATUS_UNSPECIFIED : results.first().status(); }
};

} // namespace

class VehicleServerTest : public QObject
{
    Q_OBJECT

private:
    VehicleServer::Options options(bool requireApproval) const
    {
        VehicleServer::Options o;
        o.driverPath = QStringLiteral("C:/fake/driver.dll");
        o.deviceHostPath = QCoreApplication::applicationFilePath();   // see runFakeHost()
        o.secret = kSecret;
        o.adapterName = QStringLiteral("Fake adapter");
        o.requireApproval = requireApproval;
        return o;
    }

private slots:
    void quickTunnelHost()
    {
        QCOMPARE(CloudflaredTunnel::parseQuickTunnelHost(
                     "2026-10-13T06:05:00Z INF |  https://brave-cat-xyz.trycloudflare.com                     |"),
                 QStringLiteral("brave-cat-xyz.trycloudflare.com"));
        QCOMPARE(CloudflaredTunnel::parseQuickTunnelHost("INF Requesting new quick Tunnel on trycloudflare.com..."), QString());
        QCOMPARE(CloudflaredTunnel::parseQuickTunnelHost(
                     "ERR failed to request quick Tunnel: Post \"https://api.trycloudflare.com/tunnel\": EOF"),
                 QString());
        QCOMPARE(CloudflaredTunnel::shortName("brave-cat-xyz.trycloudflare.com"), QStringLiteral("brave-cat-xyz"));
        QCOMPARE(CloudflaredTunnel::shortName("adapter.example.com"), QStringLiteral("adapter.example.com"));
    }

    void rejectRefusesWithoutStartingHost()
    {
        VehicleServer server;
        QString error;
        QVERIFY2(server.start(options(true), &error), qPrintable(error));
        QSignalSpy asked(&server, &VehicleServer::approvalRequested);
        QSignalSpy connected(&server, &VehicleServer::clientConnected);

        Client client(server.port());
        QVERIFY(client.hello(kSecret, "WORKSHOP-PC"));
        QVERIFY(asked.wait(5000));
        QCOMPARE(asked.first().first().toString(), QStringLiteral("WORKSHOP-PC"));
        QVERIFY(server.awaitingApproval());
        QVERIFY(client.results.isEmpty());   // nothing until someone answers

        server.reject();
        QVERIFY(client.waitForResult());
        QCOMPARE(client.status(), HelloResult::Status::STATUS_REJECTED);
        QVERIFY(client.results.first().message().contains("declined"));
        QVERIFY(!server.awaitingApproval());
        QCOMPARE(connected.count(), 0);
    }

    void allowStartsHostAndAccepts()
    {
        VehicleServer server;
        QString error;
        QVERIFY2(server.start(options(true), &error), qPrintable(error));
        QSignalSpy asked(&server, &VehicleServer::approvalRequested);
        QSignalSpy connected(&server, &VehicleServer::clientConnected);
        QSignalSpy disconnected(&server, &VehicleServer::clientDisconnected);

        Client client(server.port());
        QVERIFY(client.hello(kSecret, "WORKSHOP-PC"));
        QVERIFY(asked.wait(5000));

        server.approve();
        QVERIFY(client.waitForResult());
        QCOMPARE(client.status(), HelloResult::Status::STATUS_ACCEPTED);
        QCOMPARE(client.results.first().adapterName(), QStringLiteral("Fake adapter"));
        QCOMPARE(connected.count(), 1);
        QVERIFY(server.hasClient());

        client.socket.close();
        QVERIFY(disconnected.wait(5000));
        QVERIFY(!server.hasClient());
    }

    void clientLeavingWhileAskingCancels()
    {
        VehicleServer server;
        QString error;
        QVERIFY2(server.start(options(true), &error), qPrintable(error));
        QSignalSpy asked(&server, &VehicleServer::approvalRequested);
        QSignalSpy cancelled(&server, &VehicleServer::approvalCancelled);

        {
            Client client(server.port());
            QVERIFY(client.hello(kSecret, "WORKSHOP-PC"));
            QVERIFY(asked.wait(5000));
            client.socket.close();
        }
        QVERIFY(cancelled.count() == 1 || cancelled.wait(5000));
        QVERIFY(!server.awaitingApproval());

        // A late answer is ignored, and the next client can ask again.
        server.approve();
        Client next(server.port());
        QVERIFY(next.hello(kSecret, "OTHER-PC"));
        QVERIFY(asked.wait(5000));
        QCOMPARE(asked.last().first().toString(), QStringLiteral("OTHER-PC"));
    }

    void noApprovalNeededHeadless()
    {
        VehicleServer server;
        QString error;
        QVERIFY2(server.start(options(false), &error), qPrintable(error));
        QSignalSpy asked(&server, &VehicleServer::approvalRequested);

        Client client(server.port());
        QVERIFY(client.hello(kSecret, "WORKSHOP-PC"));
        QVERIFY(client.waitForResult());
        QCOMPARE(client.status(), HelloResult::Status::STATUS_ACCEPTED);
        QCOMPARE(asked.count(), 0);
    }

    void wrongSecretsLockOut()
    {
        VehicleServer server;
        QString error;
        QVERIFY2(server.start(options(true), &error), qPrintable(error));
        QSignalSpy asked(&server, &VehicleServer::approvalRequested);
        QSignalSpy locked(&server, &VehicleServer::lockedOut);

        for (int i = 0; i < VehicleServer::kMaxSecretFailures; ++i) {
            Client wrong(server.port());
            QVERIFY(wrong.hello(QStringLiteral("WRONG-%1").arg(i), "GUESSER"));
            QVERIFY(wrong.waitForResult());
            QCOMPARE(wrong.status(), HelloResult::Status::STATUS_BAD_SECRET);
        }
        QCOMPARE(locked.count(), 1);

        Client right(server.port());
        QVERIFY(right.waitForResult());   // refused on connect, before Hello
        QCOMPARE(right.status(), HelloResult::Status::STATUS_REJECTED);
        QCOMPARE(asked.count(), 0);
    }
};

int main(int argc, char *argv[])
{
    if (argc > 2 && std::strcmp(argv[1], "--pipe") == 0)
        return runFakeHost(argc, argv);

    QCoreApplication app(argc, argv);
    VehicleServerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "vehicle_server_test.moc"
