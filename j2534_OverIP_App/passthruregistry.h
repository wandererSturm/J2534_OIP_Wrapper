#ifndef PASSTHRUREGISTRY_H
#define PASSTHRUREGISTRY_H

#include <QList>
#include <QString>
#include <QStringList>

// Registry settings shared by the PassThru DLL and the App.
//
// J2534 drivers register under
//   HKLM\SOFTWARE\WOW6432Node\PassThruSupport.04.04\<driver>   (and .05.00)
// Ours is <driver> = XplatformsPassThruOverIP; besides the standard values
// (Name, Vendor, FunctionLibrary, ConfigApplication, protocol flags) it holds
// ExSelectedInterface and WrappedDll, which DllMain reads.
//
// `root` parameters default to the real HKLM key; tests pass an HKCU key.
namespace PassThruRegistry
{
inline const QString kDefaultRoot =
    QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\PassThruSupport.04.04");
inline const QString kDriverKeyName = QStringLiteral("XplatformsPassThruOverIP");

inline const QString kCannelloni = QStringLiteral("J2534Cannelloni");
inline const QString kDllWrapper = QStringLiteral("J2534DllWrapper");
inline const QString kEmulator = QStringLiteral("J2534Emulator");
// Diagnostic PC: the adapter is on a Vehicle PC (connection code in HKCU).
inline const QString kRemote = QStringLiteral("J2534Remote");

bool isKnownInterface(const QString &name);

struct Config
{
    QString selectedInterface;
    QString wrappedDll;
    QString functionLibrary;
    QString configApplication;
};

Config read(const QString &root = kDefaultRoot);

struct Driver
{
    QString name;
    QString vendor;
    QString path;
    QString apiVersion;     // "04.04" or "05.00"
};

// The HKLM PassThruSupport.04.04 and .05.00 keys.
QStringList defaultDriverRoots();
// Installed J2534 drivers except our own, de-duplicated by DLL path.
QList<Driver> installedDrivers(const QStringList &roots = defaultDriverRoots());

// Empty if `path` is a 32-bit (x86) DLL the PassThru DLL can load,
// otherwise a message for the user.
QString checkDriverArchitecture(const QString &path);

struct ApplyRequest
{
    QString interfaceName;
    QString wrappedDll;         // required for J2534DllWrapper
    QString functionLibrary;    // our x86 DLL; registered if not already
    QString configApplication;  // this App
};

// Validates and writes the request. Returns false and sets `error` on failure.
bool apply(const ApplyRequest &request, QString *error, const QString &root = kDefaultRoot);

// `reg add` commands equivalent to apply(), for manual setup.
QString manualCommands(const ApplyRequest &request);

// Elevated helper mode: `--apply-config --interface <name> [--wrapped-dll <path>]
// [--function-library <path>] [--config-application <path>] [--result-file <path>]
// [--root <key>]`. Writes a message to the result file; returns the exit code.
int runApplyFromArguments(const QStringList &arguments);
}

#endif // PASSTHRUREGISTRY_H
