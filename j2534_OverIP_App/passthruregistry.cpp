#include "passthruregistry.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QSettings>
#include <QtEndian>

namespace PassThruRegistry
{

namespace {

QString driverKey(const QString &root)
{
    return root + QStringLiteral("\\") + kDriverKeyName;
}

QString samePathKey(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path)).toLower();
}

// Supported protocols advertised to host applications (J2534 04.04 values).
const QStringList kProtocolFlags{QStringLiteral("CAN"), QStringLiteral("ISO15765"), QStringLiteral("ISO9141")};

} // namespace

bool isKnownInterface(const QString &name)
{
    return name == kCannelloni || name == kDllWrapper || name == kEmulator || name == kRemote;
}

Config read(const QString &root)
{
    QSettings s(driverKey(root), QSettings::NativeFormat);
    return Config{
        s.value(QStringLiteral("ExSelectedInterface")).toString(),
        s.value(QStringLiteral("WrappedDll")).toString(),
        s.value(QStringLiteral("FunctionLibrary")).toString(),
        s.value(QStringLiteral("ConfigApplication")).toString(),
    };
}

QStringList defaultDriverRoots()
{
    return {
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\PassThruSupport.04.04"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\PassThruSupport.05.00"),
    };
}

QList<Driver> installedDrivers(const QStringList &roots)
{
    QList<Driver> drivers;
    QSet<QString> seen;

    for (const auto &root : roots) {
        const QString apiVersion = root.section(QLatin1Char('.'), -2);   // "04.04"
        QSettings s(root, QSettings::NativeFormat);
        for (const auto &group : s.childGroups()) {
            if (group.compare(kDriverKeyName, Qt::CaseInsensitive) == 0)
                continue;
            s.beginGroup(group);
            const QString path = s.value(QStringLiteral("FunctionLibrary")).toString();
            if (!path.isEmpty() && !seen.contains(samePathKey(path))) {
                seen.insert(samePathKey(path));
                drivers.append(Driver{
                    s.value(QStringLiteral("Name"), group).toString(),
                    s.value(QStringLiteral("Vendor")).toString(),
                    path,
                    apiVersion,
                });
            }
            s.endGroup();
        }
    }
    return drivers;
}

QString checkDriverArchitecture(const QString &path)
{
    if (path.isEmpty())
        return QStringLiteral("Choose the driver DLL to wrap.");

    QFile file(path);
    if (!file.exists())
        return QStringLiteral("File not found: %1").arg(QDir::toNativeSeparators(path));
    if (!file.open(QFile::ReadOnly))
        return QStringLiteral("Can't read %1: %2").arg(QDir::toNativeSeparators(path), file.errorString());

    // DOS header: "MZ", offset of the PE header at 0x3C.
    const QByteArray dos = file.read(0x40);
    if (dos.size() < 0x40 || !dos.startsWith("MZ"))
        return QStringLiteral("%1 is not a Windows DLL.").arg(QDir::toNativeSeparators(path));
    const quint32 peOffset = qFromLittleEndian<quint32>(dos.constData() + 0x3C);

    // PE header: "PE\0\0", then the machine type.
    if (!file.seek(peOffset))
        return QStringLiteral("%1 is not a Windows DLL.").arg(QDir::toNativeSeparators(path));
    const QByteArray pe = file.read(6);
    if (pe.size() < 6 || !pe.startsWith(QByteArray("PE\0\0", 4)))
        return QStringLiteral("%1 is not a Windows DLL.").arg(QDir::toNativeSeparators(path));

    const quint16 machine = qFromLittleEndian<quint16>(pe.constData() + 4);
    switch (machine) {
    case 0x014c:    // IMAGE_FILE_MACHINE_I386
        return {};
    case 0x8664:    // IMAGE_FILE_MACHINE_AMD64
        return QStringLiteral("%1 is a 64-bit DLL. The PassThru DLL is 32-bit and can only wrap 32-bit J2534 drivers.")
            .arg(QDir::toNativeSeparators(path));
    default:
        return QStringLiteral("%1 is built for an unsupported CPU (machine type 0x%2).")
            .arg(QDir::toNativeSeparators(path))
            .arg(machine, 4, 16, QLatin1Char('0'));
    }
}

bool apply(const ApplyRequest &request, QString *error, const QString &root)
{
    auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return false;
    };

    if (!isKnownInterface(request.interfaceName))
        return fail(QStringLiteral("Unknown interface \"%1\".").arg(request.interfaceName));
    if (request.interfaceName == kDllWrapper) {
        const QString problem = checkDriverArchitecture(request.wrappedDll);
        if (!problem.isEmpty())
            return fail(problem);
    }

    QSettings s(driverKey(root), QSettings::NativeFormat);
    s.setValue(QStringLiteral("ExSelectedInterface"), request.interfaceName);
    if (request.interfaceName == kDllWrapper)
        s.setValue(QStringLiteral("WrappedDll"), QDir::toNativeSeparators(request.wrappedDll));

    // Register our DLL so host applications list it, unless it already is.
    const QString current = s.value(QStringLiteral("FunctionLibrary")).toString();
    if (!request.functionLibrary.isEmpty()
        && samePathKey(current) != samePathKey(request.functionLibrary)) {
        s.setValue(QStringLiteral("Name"), QStringLiteral("J2534 Over IP (Xplatforms)"));
        s.setValue(QStringLiteral("Vendor"), QStringLiteral("Xplatforms"));
        s.setValue(QStringLiteral("FunctionLibrary"), QDir::toNativeSeparators(request.functionLibrary));
        if (!request.configApplication.isEmpty())
            s.setValue(QStringLiteral("ConfigApplication"), QDir::toNativeSeparators(request.configApplication));
        for (const auto &flag : kProtocolFlags)
            s.setValue(flag, 1);
    }

    s.sync();
    if (s.status() != QSettings::NoError)
        return fail(QStringLiteral("Can't write %1. Administrator rights are required.").arg(driverKey(root)));
    return true;
}

QString manualCommands(const ApplyRequest &request)
{
    const QString key = QStringLiteral("HKLM\\SOFTWARE\\WOW6432Node\\PassThruSupport.04.04\\") + kDriverKeyName;
    auto regSz = [&key](const QString &name, const QString &value) {
        return QStringLiteral("reg add \"%1\" /v %2 /t REG_SZ /d \"%3\" /f").arg(key, name, value);
    };

    QStringList lines;
    lines << regSz(QStringLiteral("ExSelectedInterface"), request.interfaceName);
    if (request.interfaceName == kDllWrapper) {
        lines << regSz(QStringLiteral("WrappedDll"),
                       request.wrappedDll.isEmpty() ? QStringLiteral("<path to the driver .dll>")
                                                    : QDir::toNativeSeparators(request.wrappedDll));
    }
    if (!request.functionLibrary.isEmpty()) {
        lines << regSz(QStringLiteral("Name"), QStringLiteral("J2534 Over IP (Xplatforms)"))
              << regSz(QStringLiteral("Vendor"), QStringLiteral("Xplatforms"))
              << regSz(QStringLiteral("FunctionLibrary"), QDir::toNativeSeparators(request.functionLibrary));
        if (!request.configApplication.isEmpty())
            lines << regSz(QStringLiteral("ConfigApplication"), QDir::toNativeSeparators(request.configApplication));
        for (const auto &flag : kProtocolFlags)
            lines << QStringLiteral("reg add \"%1\" /v %2 /t REG_DWORD /d 1 /f").arg(key, flag);
    }
    return lines.join(QLatin1Char('\n'));
}

int runApplyFromArguments(const QStringList &arguments)
{
    QCommandLineParser parser;
    const QCommandLineOption applyOption(QStringLiteral("apply-config"));
    const QCommandLineOption interfaceOption(QStringLiteral("interface"), {}, QStringLiteral("name"));
    const QCommandLineOption wrappedOption(QStringLiteral("wrapped-dll"), {}, QStringLiteral("path"));
    const QCommandLineOption libraryOption(QStringLiteral("function-library"), {}, QStringLiteral("path"));
    const QCommandLineOption appOption(QStringLiteral("config-application"), {}, QStringLiteral("path"));
    const QCommandLineOption resultOption(QStringLiteral("result-file"), {}, QStringLiteral("path"));
    const QCommandLineOption rootOption(QStringLiteral("root"), {}, QStringLiteral("key"));
    parser.addOptions({applyOption, interfaceOption, wrappedOption, libraryOption, appOption, resultOption, rootOption});

    QString message;
    bool ok = false;
    if (!parser.parse(arguments)) {
        message = parser.errorText();
    } else {
        const ApplyRequest request{
            parser.value(interfaceOption),
            parser.value(wrappedOption),
            parser.value(libraryOption),
            parser.value(appOption),
        };
        const QString root = parser.isSet(rootOption) ? parser.value(rootOption) : kDefaultRoot;
        ok = apply(request, &message, root);
    }

    if (parser.isSet(resultOption)) {
        QFile result(parser.value(resultOption));
        if (result.open(QFile::WriteOnly | QFile::Truncate))
            result.write(message.toUtf8());
    }
    return ok ? 0 : 1;
}

} // namespace PassThruRegistry
