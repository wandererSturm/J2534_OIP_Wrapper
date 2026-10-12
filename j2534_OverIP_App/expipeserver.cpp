#include "defs.h"
#include "types.h"
#include "expipeserver.h"
#include <QLocalSocket>
#include <QProtobufSerializer>
#include <QTimer>

#include "expipeserver.h"
#include "exmessagepreprocessor.h"

ExPipeServer* ExPipeServer::instance = nullptr;
QMutex ExPipeServer::mutex;

ExPipeServer::ExPipeServer(QObject *parent) : QLocalServer(parent) {
    connect(this, &ExPipeServer::newConnection, this, &ExPipeServer::handleNewConnection);
    //this->setSocketOptions(OtherAccessOption);
}

ExPipeServer* ExPipeServer::getInstance() {
    mutex.lock();
    if (instance == nullptr) {
        instance = new ExPipeServer();
        instance->setSocketOptions(QLocalServer::WorldAccessOption); // Setting the socket option
    }
    mutex.unlock();
    return instance;
}

bool ExPipeServer::startServer(const QString &serverName) {
    if (listen(serverName)) {
        qDbg() << "Server started, listening to" << this->fullServerName();
        return true;
    } else {
        qDbg() << "Server failed to start:" << errorString();
        close();
        return false;
    }
}

void ExPipeServer::stopServer() {
    close();
}

void ExPipeServer::handleNewConnection() {
    qDbg() << "handleNewConnection";
    while (hasPendingConnections()) {
        QLocalSocket *clientSocket = nextPendingConnection();
        connect(clientSocket, &QLocalSocket::readyRead, this, [=, this](){ handleSocketReadyRead(clientSocket); });
        connect(clientSocket, &QLocalSocket::disconnected, this, &ExPipeServer::handleSocketDisconnected);
    }
}

void ExPipeServer::handleSocketReadyRead(QLocalSocket *clientSocket) {
    processData(clientSocket);
}

void ExPipeServer::handleSocketDisconnected() {
    auto *clientSocket = static_cast<QLocalSocket*>(sender());
    if (clientSocket) {
        m_buffers.remove(clientSocket);
        m_sizes.remove(clientSocket);
        clientSocket->deleteLater();
    }
}

void ExPipeServer::processData(QLocalSocket *clientSocket) {
    qDbg() << "processData";

    QByteArray &buffer = m_buffers[clientSocket];
    qint32 &size = m_sizes[clientSocket];

    while (clientSocket->bytesAvailable())
    {
        buffer.append(clientSocket->readAll());

        while ((size == 0 && buffer.size() >= 4) || (size > 0 && buffer.size() >= size)) {
            if (size == 0 && buffer.size() >= 4) {
                size = *reinterpret_cast<const qint32*>(buffer.constData());
                buffer.remove(0, 4);
            }

            if (size > 0 && buffer.size() >= size)
            {
                QByteArray data = buffer.mid(0, size);
                buffer.remove(0, size);
                size = 0;

                QProtobufSerializer serializer;
                PipeMessage msg;
                if (!msg.deserialize(&serializer, data))
                {
                    qDbg() << "Error: can't decode pipe message:" << serializer.lastErrorString();
                    continue;
                }

                if (msg.version() != j2534::pipe::VersionGadget::Version::VERSION_CURRENT)
                {
                    qDbg() << "Warning: DLL sent pipe protocol version" << msg.version()
                           << ", this App expects" << j2534::pipe::VersionGadget::Version::VERSION_CURRENT
                           << ". Update the DLL or the App so they match.";
                }

                QTimer::singleShot(0,[msg](){ExMessagePreProcessor::getInstance()->incommingMessageFromClient(msg);});
            }
        }
    }
}


