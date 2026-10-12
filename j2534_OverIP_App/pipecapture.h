#ifndef PIPECAPTURE_H
#define PIPECAPTURE_H

#include <QList>
#include <QString>

#include "types.h"

// Saves and loads captured pipe messages (export.json).
//
// File format 2:
//   { "format": 2, "messages": [ <PipeMessage as protobuf JSON>, ... ] }
//
// Captures from before the protobuf pipe (format 1, CBOR payloads) are rejected.
namespace PipeCapture
{
constexpr int kFormat = 2;

bool save(const QString &path, const QList<PipeMessage> &messages, QString *error = nullptr);
bool load(const QString &path, QList<PipeMessage> *messages, QString *error = nullptr);
}

#endif // PIPECAPTURE_H
