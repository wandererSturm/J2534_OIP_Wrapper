#include "defs.h"

#include "exmessagepreprocessor.h"
#include "exj2534emulator.h"
#include "exj2534wrapper.h"
#include "exj2534cannelloni.h"


ExMessagePreProcessor::ExMessagePreProcessor(QObject *parent):QObject{parent}
{

}

ExMessagePreProcessor * ExMessagePreProcessor::getInstance()
{
    if(ExMessagePreProcessor::s_singletonInstance == nullptr)ExMessagePreProcessor::s_singletonInstance = new ExMessagePreProcessor();
    return ExMessagePreProcessor::s_singletonInstance;
}

void ExMessagePreProcessor::incommingMessageFromClient(PipeMessage msg)
{
    qDbg() << "[MSG]:" << msg.processPath() << " id:" << msg.clientId() << " tstamp: " << msg.timestampMs()
           << " module:" << msg.module() << " text:" << msg.text() << " payload:" << msg.payloadField();

    switch(msg.module())
    {
    case PipeModule::MODULE_DLL_WRAPPER:
        ExJ2534Wrapper::getInstance()->processPipeMessage(msg);
        break;
    case PipeModule::MODULE_EMULATOR:
        ExJ2534Emulator::getInstance()->processPipeMessage(msg);
        break;
    case PipeModule::MODULE_CANNELLONI:
        ExJ2534Cannelloni::getInstance()->processPipeMessage(msg);
        break;
    default:
        break;
    }

}

ExMessagePreProcessor * ExMessagePreProcessor::create(QQmlEngine *, QJSEngine *engine)
{
    // The instance has to exist before it is used. We cannot replace it.
    Q_ASSERT(s_singletonInstance);

    // The engine has to have the same thread affinity as the singleton.
    Q_ASSERT(engine->thread() == s_singletonInstance->thread());

    // There can only be one engine accessing the singleton.
    if (s_engine)
        Q_ASSERT(engine == s_engine);
    else
        s_engine = engine;

    // Explicitly specify C++ ownership so that the engine doesn't delete
    // the instance.
    QJSEngine::setObjectOwnership(s_singletonInstance, QJSEngine::CppOwnership);
    return s_singletonInstance;
}
