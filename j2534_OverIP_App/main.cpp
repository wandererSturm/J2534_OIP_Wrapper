#include "defs.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngineExtensionPlugin>
#include <QQmlContext>
#include <cstring>

#include "expipeserver.h"
#include "exj2534emulator.h"
#include "exj2534wrapper.h"
#include "exj2534cannelloni.h"
#include "passthruregistry.h"

int main(int argc, char *argv[])
{
    // Elevated helper started by InterfaceConfig::apply(): write the registry, no UI.
    if (argc > 1 && std::strcmp(argv[1], "--apply-config") == 0)
    {
        QCoreApplication helper(argc, argv);
        return PassThruRegistry::runApplyFromArguments(helper.arguments());
    }

    QGuiApplication app(argc, argv);

    ExPipeServer * server = ExPipeServer::getInstance();//"\\\\.\\pipe\\exj2534_overip_pipe");
    if (!server->startServer("\\\\.\\pipe\\exj2534_overip_pipe")) {
        qDbg() << "Another instance is already running";
    }
    else qDbg() << "Server started";


    qmlRegisterSingletonInstance("ExJ2534Emulator", 1, 0, "ExJ2534Emulator", ExJ2534Emulator::getInstance());
    qmlRegisterSingletonInstance("ExJ2534Wrapper", 1, 0, "ExJ2534Wrapper", ExJ2534Wrapper::getInstance());
    qmlRegisterSingletonInstance("ExJ2534Cannelloni", 1, 0, "ExJ2534Cannelloni", ExJ2534Cannelloni::getInstance());

    // The selected PassThru interface comes from the InterfaceConfig QML singleton.
    QQmlApplicationEngine engine;

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("de.xplatforms.j2534", "Main");


    return app.exec();
}
