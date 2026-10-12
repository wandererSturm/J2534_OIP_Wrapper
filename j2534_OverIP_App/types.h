#ifndef TYPES_H
#define TYPES_H

#include <QObject>

#include "j2534_pipe.qpb.h"

// One message from the PassThru DLL, as defined in proto/j2534_pipe.proto.
using PipeMessage = j2534::pipe::PipeMessage;
using PipePayload = j2534::pipe::PipeMessage::PayloadFields;
using PipeModule = j2534::pipe::ModuleGadget::Module;

struct TimestampedObject {
    qint64 timestamp;
    PipeMessage msg;
    QString data;
    bool hasError = false;
};


#endif // TYPES_H
