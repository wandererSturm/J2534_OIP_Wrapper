#include "pipecapture.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProtobufJsonSerializer>

namespace PipeCapture
{

namespace {

void setError(QString *error, const QString &text)
{
    if (error)
        *error = text;
}

} // namespace

bool save(const QString &path, const QList<PipeMessage> &messages, QString *error)
{
    QProtobufJsonSerializer serializer;
    QJsonArray array;
    for (const auto &msg : messages)
        array.append(QJsonDocument::fromJson(msg.serialize(&serializer)).object());

    const QJsonObject root{
        {QStringLiteral("format"), kFormat},
        {QStringLiteral("messages"), array},
    };

    QFile file(path);
    if (!file.open(QFile::WriteOnly)) {
        setError(error, QStringLiteral("Can't write %1: %2").arg(path, file.errorString()));
        return false;
    }
    file.write(QJsonDocument(root).toJson());
    return true;
}

bool load(const QString &path, QList<PipeMessage> *messages, QString *error)
{
    QFile file(path);
    if (!file.open(QFile::ReadOnly)) {
        setError(error, QStringLiteral("Can't read %1: %2").arg(path, file.errorString()));
        return false;
    }

    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (doc.isNull()) {
        setError(error, QStringLiteral("%1 is not valid JSON: %2").arg(path, parseError.errorString()));
        return false;
    }

    // Format 1 was a bare array of CBOR-based messages.
    if (!doc.isObject() || doc.object().value(QStringLiteral("format")).toInt() != kFormat) {
        setError(error, QStringLiteral("%1 was saved by an older version (CBOR pipe format) and "
                                       "can't be loaded. Capture it again with this version.").arg(path));
        return false;
    }

    QProtobufJsonSerializer serializer;
    const auto array = doc.object().value(QStringLiteral("messages")).toArray();
    QList<PipeMessage> loaded;
    loaded.reserve(array.size());
    for (qsizetype i = 0; i < array.size(); ++i) {
        PipeMessage msg;
        const auto json = QJsonDocument(array.at(i).toObject()).toJson(QJsonDocument::Compact);
        if (!msg.deserialize(&serializer, json)) {
            setError(error, QStringLiteral("%1: message %2 is invalid: %3")
                                .arg(path).arg(i).arg(serializer.lastErrorString()));
            return false;
        }
        loaded.append(msg);
    }

    *messages = std::move(loaded);
    return true;
}

} // namespace PipeCapture
