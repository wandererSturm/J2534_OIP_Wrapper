// PassThruRegistry against a throwaway HKCU key (no admin rights needed).

#include <QTest>
#include <QCoreApplication>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QtEndian>

#include "passthruregistry.h"

namespace {

const QString kTestBase = QStringLiteral("HKEY_CURRENT_USER\\Software\\J2534OIPWrapperTest");
const QString kRoot0404 = kTestBase + QStringLiteral("\\PassThruSupport.04.04");
const QString kRoot0500 = kTestBase + QStringLiteral("\\PassThruSupport.05.00");

QString ourKey() { return kRoot0404 + QStringLiteral("\\") + PassThruRegistry::kDriverKeyName; }

// Minimal PE image: DOS header pointing at a PE header with the given machine type.
QString writeFakeDll(const QTemporaryDir &dir, const QString &name, quint16 machine)
{
    QByteArray image(0x80, '\0');
    image[0] = 'M';
    image[1] = 'Z';
    qToLittleEndian<quint32>(0x40, image.data() + 0x3C);
    image.replace(0x40, 4, QByteArray("PE\0\0", 4));
    qToLittleEndian<quint16>(machine, image.data() + 0x44);

    const QString path = dir.filePath(name);
    QFile file(path);
    if (!file.open(QFile::WriteOnly))
        return {};
    file.write(image);
    return path;
}

void setDriver(const QString &root, const QString &key, const QString &name, const QString &path)
{
    QSettings s(root + QStringLiteral("\\") + key, QSettings::NativeFormat);
    s.setValue(QStringLiteral("Name"), name);
    if (!path.isEmpty())
        s.setValue(QStringLiteral("FunctionLibrary"), path);
}

} // namespace

class PassThruRegistryTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    QString m_x86Dll;
    QString m_x64Dll;

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        m_x86Dll = writeFakeDll(m_dir, QStringLiteral("driver32.dll"), 0x014c);
        m_x64Dll = writeFakeDll(m_dir, QStringLiteral("driver64.dll"), 0x8664);
    }

    void init() { QSettings(kTestBase, QSettings::NativeFormat).remove(QString()); }
    void cleanupTestCase() { QSettings(kTestBase, QSettings::NativeFormat).remove(QString()); }

    void architectureCheck()
    {
        QVERIFY(PassThruRegistry::checkDriverArchitecture(m_x86Dll).isEmpty());
        QVERIFY(PassThruRegistry::checkDriverArchitecture(m_x64Dll).contains("64-bit"));
        // This test binary is a real x64 PE file.
        QVERIFY(PassThruRegistry::checkDriverArchitecture(QCoreApplication::applicationFilePath()).contains("64-bit"));

        QFile junk(m_dir.filePath("junk.dll"));
        QVERIFY(junk.open(QFile::WriteOnly));
        junk.write("not a dll");
        junk.close();
        QVERIFY(PassThruRegistry::checkDriverArchitecture(junk.fileName()).contains("not a Windows DLL"));

        QVERIFY(PassThruRegistry::checkDriverArchitecture(m_dir.filePath("missing.dll")).contains("not found"));
        QVERIFY(!PassThruRegistry::checkDriverArchitecture(QString()).isEmpty());
    }

    void applyWritesSelectionAndRegisters()
    {
        QString error;
        QVERIFY2(PassThruRegistry::apply({PassThruRegistry::kCannelloni, {}, "C:/dist/x86/j2534_OverIP.dll",
                                          "C:/dist/x64/bin/j2534_OverIP_App.exe"}, &error, kRoot0404),
                 qPrintable(error));

        const auto config = PassThruRegistry::read(kRoot0404);
        QCOMPARE(config.selectedInterface, PassThruRegistry::kCannelloni);
        QCOMPARE(config.functionLibrary, QStringLiteral("C:\\dist\\x86\\j2534_OverIP.dll"));
        QCOMPARE(config.configApplication, QStringLiteral("C:\\dist\\x64\\bin\\j2534_OverIP_App.exe"));

        QSettings s(ourKey(), QSettings::NativeFormat);
        QCOMPARE(s.value("CAN").toInt(), 1);
        QCOMPARE(s.value("ISO15765").toInt(), 1);
        QCOMPARE(s.value("Vendor").toString(), QStringLiteral("Xplatforms"));
    }

    void applyKeepsExistingRegistration()
    {
        {
            QSettings s(ourKey(), QSettings::NativeFormat);
            s.setValue("FunctionLibrary", "C:\\DIST\\x86\\J2534_OverIP.dll");
            s.setValue("Name", "Custom name");
        }

        QString error;
        QVERIFY(PassThruRegistry::apply({PassThruRegistry::kEmulator, {}, "C:/dist/x86/j2534_OverIP.dll", {}}, &error, kRoot0404));

        QSettings s(ourKey(), QSettings::NativeFormat);
        QCOMPARE(s.value("ExSelectedInterface").toString(), PassThruRegistry::kEmulator);
        QCOMPARE(s.value("Name").toString(), QStringLiteral("Custom name"));   // same DLL: left alone
    }

    void applyWrapperChecksDriver()
    {
        QString error;
        QVERIFY(!PassThruRegistry::apply({PassThruRegistry::kDllWrapper, m_x64Dll, {}, {}}, &error, kRoot0404));
        QVERIFY2(error.contains("64-bit"), qPrintable(error));
        QVERIFY(PassThruRegistry::read(kRoot0404).selectedInterface.isEmpty());   // nothing written

        QVERIFY2(PassThruRegistry::apply({PassThruRegistry::kDllWrapper, m_x86Dll, {}, {}}, &error, kRoot0404),
                 qPrintable(error));
        const auto config = PassThruRegistry::read(kRoot0404);
        QCOMPARE(config.selectedInterface, PassThruRegistry::kDllWrapper);
        QCOMPARE(config.wrappedDll, QDir::toNativeSeparators(m_x86Dll));
        QVERIFY(config.functionLibrary.isEmpty());   // no DLL given: not registered
    }

    void registerOnlyKeepsSelection()
    {
        QString error;
        QVERIFY(PassThruRegistry::apply({PassThruRegistry::kRemote, {}, {}, {}}, &error, kRoot0404));

        // The installer: register (a new path) without choosing an interface.
        QVERIFY2(PassThruRegistry::apply({{}, {}, "C:/Program Files/J2534OverIP/x86/j2534_OverIP.dll",
                                          "C:/Program Files/J2534OverIP/x64/bin/j2534_OverIP_App.exe"}, &error, kRoot0404),
                 qPrintable(error));
        const auto config = PassThruRegistry::read(kRoot0404);
        QCOMPARE(config.selectedInterface, PassThruRegistry::kRemote);
        QCOMPARE(config.functionLibrary, QStringLiteral("C:\\Program Files\\J2534OverIP\\x86\\j2534_OverIP.dll"));

        QVERIFY(!PassThruRegistry::apply({{}, {}, {}, {}}, &error, kRoot0404));
        QVERIFY(error.contains("Nothing to do"));
    }

    void removeDeletesOnlyOurKey()
    {
        setDriver(kRoot0404, "OpenPort", "OpenPort 2.0", "C:\\Drivers\\op20pt32.dll");
        QString error;
        QVERIFY(PassThruRegistry::apply({PassThruRegistry::kEmulator, {}, "C:/dist/x86/j2534_OverIP.dll", {}}, &error, kRoot0404));

        const QString resultFile = m_dir.filePath("remove.txt");
        QCOMPARE(PassThruRegistry::runApplyFromArguments(
                     {"app.exe", "--apply-config", "--remove", "--result-file", resultFile, "--root", kRoot0404}), 0);
        QVERIFY(PassThruRegistry::read(kRoot0404).selectedInterface.isEmpty());
        QVERIFY(PassThruRegistry::read(kRoot0404).functionLibrary.isEmpty());
        QCOMPARE(PassThruRegistry::installedDrivers({kRoot0404}).size(), 1);   // other drivers stay

        QVERIFY(PassThruRegistry::remove(&error, kRoot0404));   // already gone: fine
    }

    void applyRejectsUnknownInterface()
    {
        QString error;
        QVERIFY(!PassThruRegistry::apply({"SomethingElse", {}, {}, {}}, &error, kRoot0404));
        QVERIFY(error.contains("Unknown interface"));
    }

    void installedDriversListsOthers()
    {
        setDriver(kRoot0404, "OpenPort", "OpenPort 2.0", "C:\\Drivers\\op20pt32.dll");
        setDriver(kRoot0404, PassThruRegistry::kDriverKeyName, "Ours", "C:\\dist\\x86\\j2534_OverIP.dll");
        setDriver(kRoot0404, "Broken", "No library", QString());
        setDriver(kRoot0500, "OpenPort", "OpenPort 2.0 (v05)", "c:\\drivers\\OP20PT32.dll");   // same DLL
        setDriver(kRoot0500, "Mongoose", "Mongoose Pro", "C:\\Drivers\\mongoose.dll");

        const auto drivers = PassThruRegistry::installedDrivers({kRoot0404, kRoot0500});
        QCOMPARE(drivers.size(), 2);
        QCOMPARE(drivers.at(0).name, QStringLiteral("OpenPort 2.0"));
        QCOMPARE(drivers.at(0).apiVersion, QStringLiteral("04.04"));
        QCOMPARE(drivers.at(1).name, QStringLiteral("Mongoose Pro"));
        QCOMPARE(drivers.at(1).apiVersion, QStringLiteral("05.00"));
    }

    void helperCommandLine()
    {
        const QString resultFile = m_dir.filePath("result.txt");
        const QStringList ok{"app.exe", "--apply-config", "--interface", PassThruRegistry::kEmulator,
                             "--result-file", resultFile, "--root", kRoot0404};
        QCOMPARE(PassThruRegistry::runApplyFromArguments(ok), 0);
        QCOMPARE(PassThruRegistry::read(kRoot0404).selectedInterface, PassThruRegistry::kEmulator);

        const QStringList bad{"app.exe", "--apply-config", "--interface", PassThruRegistry::kDllWrapper,
                              "--wrapped-dll", m_x64Dll, "--result-file", resultFile, "--root", kRoot0404};
        QCOMPARE(PassThruRegistry::runApplyFromArguments(bad), 1);
        QFile result(resultFile);
        QVERIFY(result.open(QFile::ReadOnly));
        QVERIFY(QString::fromUtf8(result.readAll()).contains("64-bit"));
        QCOMPARE(PassThruRegistry::read(kRoot0404).selectedInterface, PassThruRegistry::kEmulator);   // unchanged
    }

    void manualCommands()
    {
        const QString text = PassThruRegistry::manualCommands({PassThruRegistry::kDllWrapper, {}, "C:\\dist\\x86\\j2534_OverIP.dll", {}});
        QVERIFY(text.contains("/v ExSelectedInterface /t REG_SZ /d \"J2534DllWrapper\""));
        QVERIFY(text.contains("/v WrappedDll /t REG_SZ /d \"<path to the driver .dll>\""));
        QVERIFY(text.contains("/v FunctionLibrary /t REG_SZ /d \"C:\\dist\\x86\\j2534_OverIP.dll\""));
        QVERIFY(text.contains("/v CAN /t REG_DWORD /d 1"));
    }
};

QTEST_GUILESS_MAIN(PassThruRegistryTest)
#include "passthru_registry_test.moc"
