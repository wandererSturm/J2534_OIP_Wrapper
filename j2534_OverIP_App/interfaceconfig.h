#ifndef INTERFACECONFIG_H
#define INTERFACECONFIG_H

#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantList>

// Which PassThru interface the DLL uses (ExSelectedInterface), the wrapped
// driver, and whether our DLL is registered. apply() writes HKLM through an
// elevated copy of this App (one UAC prompt) and then reloads.
class InterfaceConfig : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString selectedInterface READ selectedInterface NOTIFY selectionChanged FINAL)
    Q_PROPERTY(int selectedModule READ selectedModule NOTIFY selectionChanged FINAL)
    Q_PROPERTY(QString wrappedDll READ wrappedDll NOTIFY configChanged FINAL)
    Q_PROPERTY(QString registeredDll READ registeredDll NOTIFY configChanged FINAL)
    Q_PROPERTY(bool registered READ registered NOTIFY configChanged FINAL)
    Q_PROPERTY(QString expectedDll READ expectedDll CONSTANT FINAL)
    Q_PROPERTY(bool expectedDllFound READ expectedDllFound CONSTANT FINAL)
    Q_PROPERTY(QVariantList drivers READ drivers NOTIFY configChanged FINAL)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged FINAL)

public:
    explicit InterfaceConfig(QObject *parent = nullptr);

    QString selectedInterface() const { return m_selectedInterface; }
    // 0 = none (SimpleLogger), 2 = DllWrapper, 3 = Emulator, 4 = Cannelloni
    int selectedModule() const;
    QString wrappedDll() const { return m_wrappedDll; }
    QString registeredDll() const { return m_registeredDll; }
    bool registered() const;
    QString expectedDll() const { return m_expectedDll; }
    bool expectedDllFound() const;
    QVariantList drivers() const { return m_drivers; }
    bool busy() const { return m_busy; }

    Q_INVOKABLE QString displayName(const QString &interfaceName) const;
    // Empty if the DLL can be wrapped, otherwise why not.
    Q_INVOKABLE QString checkDriver(const QString &path) const;
    Q_INVOKABLE QString manualCommands(const QString &interfaceName, const QString &wrappedDll) const;
    Q_INVOKABLE void apply(const QString &interfaceName, const QString &wrappedDll);
    Q_INVOKABLE void reload();
    // Asks Main.qml to show the interface chooser.
    Q_INVOKABLE void requestChooser() { emit chooserRequested(); }

signals:
    void selectionChanged();
    void configChanged();
    void busyChanged();
    void applyFinished(bool ok, const QString &message);
    void chooserRequested();

private:
    void setBusy(bool busy);
    void finishApply();

    QString m_selectedInterface;
    QString m_wrappedDll;
    QString m_registeredDll;
    QString m_expectedDll;
    QVariantList m_drivers;
    bool m_busy = false;

    // Pending elevated helper process
    void *m_process = nullptr;
    QString m_resultFile;
    QString m_pendingInterface;
    QTimer m_poll;
};

#endif // INTERFACECONFIG_H
