#include "interfaceconfig.h"
#include "passthruregistry.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <windows.h>
#include <shellapi.h>

namespace {

QString quoted(const QString &arg)
{
    return QLatin1Char('"') + arg + QLatin1Char('"');
}

QString samePath(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path)).toLower();
}

} // namespace

InterfaceConfig::InterfaceConfig(QObject *parent)
    : QObject(parent)
{
    // Install layout: dist\x64\bin\j2534_OverIP_App.exe next to dist\x86\j2534_OverIP.dll
    m_expectedDll = QDir::toNativeSeparators(QDir::cleanPath(
        QCoreApplication::applicationDirPath() + QStringLiteral("/../../x86/j2534_OverIP.dll")));

    m_poll.setInterval(100);
    connect(&m_poll, &QTimer::timeout, this, [this] {
        if (WaitForSingleObject(static_cast<HANDLE>(m_process), 0) == WAIT_OBJECT_0)
            finishApply();
    });

    reload();
}

int InterfaceConfig::selectedModule() const
{
    if (m_selectedInterface.compare(PassThruRegistry::kDllWrapper, Qt::CaseInsensitive) == 0) return 2;
    if (m_selectedInterface.compare(PassThruRegistry::kEmulator, Qt::CaseInsensitive) == 0) return 3;
    if (m_selectedInterface.compare(PassThruRegistry::kCannelloni, Qt::CaseInsensitive) == 0) return 4;
    return 0;
}

bool InterfaceConfig::registered() const
{
    return !m_registeredDll.isEmpty() && samePath(m_registeredDll) == samePath(m_expectedDll);
}

bool InterfaceConfig::expectedDllFound() const
{
    return QFileInfo::exists(m_expectedDll);
}

QString InterfaceConfig::displayName(const QString &interfaceName) const
{
    if (interfaceName == PassThruRegistry::kCannelloni) return QStringLiteral("Cannelloni (CAN over UDP)");
    if (interfaceName == PassThruRegistry::kDllWrapper) return QStringLiteral("DLL Wrapper");
    if (interfaceName == PassThruRegistry::kEmulator) return QStringLiteral("Emulator");
    return QStringLiteral("none");
}

QString InterfaceConfig::checkDriver(const QString &path) const
{
    return PassThruRegistry::checkDriverArchitecture(path);
}

QString InterfaceConfig::manualCommands(const QString &interfaceName, const QString &wrappedDll) const
{
    return PassThruRegistry::manualCommands({interfaceName, wrappedDll, m_expectedDll,
                                             QDir::toNativeSeparators(QCoreApplication::applicationFilePath())});
}

void InterfaceConfig::reload()
{
    const auto config = PassThruRegistry::read();

    QVariantList drivers;
    for (const auto &d : PassThruRegistry::installedDrivers()) {
        drivers.append(QVariantMap{
            {QStringLiteral("name"), d.name},
            {QStringLiteral("vendor"), d.vendor},
            {QStringLiteral("path"), d.path},
            {QStringLiteral("apiVersion"), d.apiVersion},
        });
    }

    const bool selectionDiffers = config.selectedInterface != m_selectedInterface;
    m_selectedInterface = config.selectedInterface;
    m_wrappedDll = config.wrappedDll;
    m_registeredDll = config.functionLibrary;
    m_drivers = drivers;

    emit configChanged();
    if (selectionDiffers)
        emit selectionChanged();
}

void InterfaceConfig::apply(const QString &interfaceName, const QString &wrappedDll)
{
    if (m_busy)
        return;

    // Same validation as the helper, so obvious mistakes don't cost a UAC prompt.
    if (interfaceName == PassThruRegistry::kDllWrapper) {
        const QString problem = checkDriver(wrappedDll);
        if (!problem.isEmpty()) {
            emit applyFinished(false, problem);
            return;
        }
    }

    m_resultFile = QDir::temp().filePath(
        QStringLiteral("j2534_apply_config_%1.txt").arg(QCoreApplication::applicationPid()));
    QFile::remove(m_resultFile);

    QStringList args{QStringLiteral("--apply-config"),
                     QStringLiteral("--interface"), interfaceName,
                     QStringLiteral("--result-file"), QDir::toNativeSeparators(m_resultFile)};
    if (interfaceName == PassThruRegistry::kDllWrapper)
        args << QStringLiteral("--wrapped-dll") << QDir::toNativeSeparators(wrappedDll);
    if (expectedDllFound()) {
        args << QStringLiteral("--function-library") << m_expectedDll
             << QStringLiteral("--config-application")
             << QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    }

    QStringList quotedArgs;
    for (const auto &arg : args)
        quotedArgs << quoted(arg);
    const std::wstring exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath()).toStdWString();
    const std::wstring params = quotedArgs.join(QLatin1Char(' ')).toStdWString();

    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = L"runas";
    info.lpFile = exe.c_str();
    info.lpParameters = params.c_str();
    info.nShow = SW_HIDE;

    if (!ShellExecuteExW(&info) || info.hProcess == nullptr) {
        const DWORD error = GetLastError();
        emit applyFinished(false, error == ERROR_CANCELLED
            ? QStringLiteral("Cancelled. Administrator rights are needed to change the interface.")
            : QStringLiteral("Couldn't start the settings helper (Windows error %1).").arg(error));
        return;
    }

    m_process = info.hProcess;
    m_pendingInterface = interfaceName;
    setBusy(true);
    m_poll.start();
}

void InterfaceConfig::finishApply()
{
    m_poll.stop();

    DWORD exitCode = 1;
    GetExitCodeProcess(static_cast<HANDLE>(m_process), &exitCode);
    CloseHandle(static_cast<HANDLE>(m_process));
    m_process = nullptr;

    QString message;
    QFile result(m_resultFile);
    if (result.open(QFile::ReadOnly))
        message = QString::fromUtf8(result.readAll());
    result.close();
    QFile::remove(m_resultFile);

    if (exitCode == 0) {
        message = QStringLiteral("PassThru interface set to %1. Restart the diagnostic program so it loads the new setting.")
                      .arg(displayName(m_pendingInterface));
    } else if (message.isEmpty()) {
        message = QStringLiteral("Changing the interface failed (exit code %1).").arg(exitCode);
    }

    setBusy(false);
    reload();
    emit applyFinished(exitCode == 0, message);
}

void InterfaceConfig::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}
