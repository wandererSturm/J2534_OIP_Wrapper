#ifndef VEHICLESHARING_H
#define VEHICLESHARING_H

#include <QObject>
#include <QQmlEngine>
#include <QStringList>
#include <QVariantList>

class CloudflaredTunnel;
class VehicleServer;

// The Vehicle PC role for QML: shares a local J2534 adapter through a
// trycloudflare tunnel and shows the connection code for the Diagnostic PC.
//
// The role is remembered per user, so the App opens as Vehicle PC again.
class VehicleSharing : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool vehicleMode READ vehicleMode NOTIFY vehicleModeChanged FINAL)
    Q_PROPERTY(State state READ state NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString connectionCode READ connectionCode NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString clientName READ clientName NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString adapterName READ adapterName NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString driverPath READ driverPath WRITE setDriverPath NOTIFY driverPathChanged FINAL)
    Q_PROPERTY(QVariantList drivers READ drivers NOTIFY driversChanged FINAL)
    Q_PROPERTY(bool cloudflaredFound READ cloudflaredFound CONSTANT FINAL)
    Q_PROPERTY(bool deviceHostFound READ deviceHostFound CONSTANT FINAL)
    Q_PROPERTY(QStringList log READ log NOTIFY logChanged FINAL)

public:
    enum class State {
        Idle,           // not sharing
        Starting,       // local server up, waiting for the tunnel
        Waiting,        // code shown, waiting for a Diagnostic PC
        AskingToAllow,  // a Diagnostic PC with the right code is waiting for Allow
        Connected,      // a Diagnostic PC is using the adapter
        Failed,         // see errorText; Start again to retry
    };
    Q_ENUM(State)

    explicit VehicleSharing(QObject *parent = nullptr);
    ~VehicleSharing() override;

    bool vehicleMode() const { return m_vehicleMode; }
    State state() const { return m_state; }
    QString connectionCode() const { return m_connectionCode; }
    QString errorText() const { return m_errorText; }
    QString clientName() const { return m_clientName; }
    QString adapterName() const { return m_adapterName; }
    QString driverPath() const { return m_driverPath; }
    void setDriverPath(const QString &path);
    QVariantList drivers() const { return m_drivers; }
    bool cloudflaredFound() const;
    bool deviceHostFound() const;
    QStringList log() const { return m_log; }

    Q_INVOKABLE void enterVehicleMode();
    Q_INVOKABLE void leaveVehicleMode();
    Q_INVOKABLE void reloadDrivers();
    // Empty if `path` can be shared, otherwise why not.
    Q_INVOKABLE QString checkDriver(const QString &path) const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void allow();
    Q_INVOKABLE void reject();
    Q_INVOKABLE void copyCode() const;

signals:
    void vehicleModeChanged();
    void stateChanged();
    void driverPathChanged();
    void driversChanged();
    void logChanged();
    // For a notification when the window is in the background.
    void attentionNeeded(const QString &text);

private:
    void setState(State state);
    void fail(const QString &message);
    void updateCode();
    void addLog(const QString &text);

    VehicleServer *m_server = nullptr;
    CloudflaredTunnel *m_tunnel = nullptr;

    bool m_vehicleMode = false;
    State m_state = State::Idle;
    QString m_secret;
    QString m_connectionCode;
    QString m_errorText;
    QString m_clientName;
    QString m_adapterName;
    QString m_driverPath;
    QVariantList m_drivers;
    QStringList m_log;
};

#endif // VEHICLESHARING_H
