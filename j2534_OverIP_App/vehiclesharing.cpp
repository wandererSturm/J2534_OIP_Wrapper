#include "vehiclesharing.h"

#include "cloudflaredtunnel.h"
#include "passthruregistry.h"
#include "vehicleserver.h"

#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QSettings>

namespace {

// Same per-user key the PassThru DLL reads RemoteConnection from.
QSettings userSettings()
{
    return QSettings(QStringLiteral("HKEY_CURRENT_USER\\Software\\Xplatforms\\J2534OverIP"), QSettings::NativeFormat);
}

const QString kRoleKey = QStringLiteral("Role");
const QString kRoleVehicle = QStringLiteral("VehiclePC");
const QString kDriverKey = QStringLiteral("VehicleDriver");
constexpr int kMaxLogLines = 200;

} // namespace

VehicleSharing::VehicleSharing(QObject *parent)
    : QObject(parent)
    , m_server(new VehicleServer(this))
    , m_tunnel(new CloudflaredTunnel(this))
{
    auto settings = userSettings();
    m_vehicleMode = settings.value(kRoleKey).toString() == kRoleVehicle;
    m_driverPath = settings.value(kDriverKey).toString();
    reloadDrivers();

    connect(m_server, &VehicleServer::logMessage, this, &VehicleSharing::addLog);
    connect(m_tunnel, &CloudflaredTunnel::logMessage, this, &VehicleSharing::addLog);

    connect(m_tunnel, &CloudflaredTunnel::stateChanged, this, [this] {
        switch (m_tunnel->state()) {
        case CloudflaredTunnel::State::Ready:
            updateCode();
            if (m_state == State::Starting)
                setState(State::Waiting);
            break;
        case CloudflaredTunnel::State::Failed:
            if (m_state != State::Idle)
                fail(m_tunnel->errorString());
            break;
        default:
            break;
        }
    });

    connect(m_server, &VehicleServer::approvalRequested, this, [this](const QString &name) {
        m_clientName = name;
        setState(State::AskingToAllow);
        emit attentionNeeded(QStringLiteral("%1 wants to use the adapter").arg(name));
    });
    connect(m_server, &VehicleServer::approvalCancelled, this, [this] {
        if (m_state == State::AskingToAllow)
            setState(State::Waiting);
    });
    connect(m_server, &VehicleServer::clientConnected, this, [this](const QString &name) {
        m_clientName = name;
        setState(State::Connected);
    });
    connect(m_server, &VehicleServer::clientDisconnected, this, [this] {
        if (m_state == State::Connected)
            setState(State::Waiting);
    });
    connect(m_server, &VehicleServer::clientRefused, this, [this] {
        if (m_state == State::AskingToAllow || m_state == State::Connected)
            setState(State::Waiting);
    });
    connect(m_server, &VehicleServer::lockedOut, this, [this] {
        fail(QStringLiteral("Someone tried %1 wrong connection codes, so sharing was stopped. "
                            "Start sharing again to get a new code.")
                 .arg(VehicleServer::kMaxSecretFailures));
    });
}

VehicleSharing::~VehicleSharing()
{
    stop();
}

void VehicleSharing::enterVehicleMode()
{
    if (m_vehicleMode)
        return;
    m_vehicleMode = true;
    userSettings().setValue(kRoleKey, kRoleVehicle);
    emit vehicleModeChanged();
}

void VehicleSharing::leaveVehicleMode()
{
    stop();
    if (!m_vehicleMode)
        return;
    m_vehicleMode = false;
    userSettings().remove(kRoleKey);
    emit vehicleModeChanged();
}

void VehicleSharing::reloadDrivers()
{
    QVariantList drivers;
    for (const auto &d : PassThruRegistry::installedDrivers()) {
        drivers.append(QVariantMap{
            {QStringLiteral("name"), d.name},
            {QStringLiteral("vendor"), d.vendor},
            {QStringLiteral("path"), d.path},
        });
    }
    m_drivers = drivers;
    emit driversChanged();
}

void VehicleSharing::setDriverPath(const QString &path)
{
    if (m_driverPath == path)
        return;
    m_driverPath = path;
    userSettings().setValue(kDriverKey, path);
    emit driverPathChanged();
}

QString VehicleSharing::checkDriver(const QString &path) const
{
    return PassThruRegistry::checkDriverArchitecture(path);
}

bool VehicleSharing::cloudflaredFound() const
{
    return !CloudflaredTunnel::findExecutable().isEmpty();
}

bool VehicleSharing::deviceHostFound() const
{
    return QFileInfo::exists(VehicleServer::defaultDeviceHostPath());
}

void VehicleSharing::start()
{
    stop();
    m_errorText.clear();

    const QString problem = checkDriver(m_driverPath);
    if (m_driverPath.isEmpty() || !problem.isEmpty()) {
        fail(m_driverPath.isEmpty() ? QStringLiteral("Choose the J2534 driver of the adapter first.") : problem);
        return;
    }

    // Adapter name for the Diagnostic PC: the registered driver name if known.
    m_adapterName = QFileInfo(m_driverPath).completeBaseName();
    for (const auto &entry : std::as_const(m_drivers)) {
        const auto driver = entry.toMap();
        if (QDir::cleanPath(driver.value(QStringLiteral("path")).toString()).compare(QDir::cleanPath(m_driverPath), Qt::CaseInsensitive) == 0)
            m_adapterName = driver.value(QStringLiteral("name")).toString();
    }

    m_secret = VehicleServer::generateSecret();
    VehicleServer::Options options;
    options.driverPath = QDir::toNativeSeparators(m_driverPath);
    options.deviceHostPath = VehicleServer::defaultDeviceHostPath();
    options.port = 0;   // any free port; only cloudflared needs to know it
    options.secret = m_secret;
    options.adapterName = m_adapterName;
    options.requireApproval = true;

    QString error;
    if (!m_server->start(options, &error)) {
        fail(error);
        return;
    }
    setState(State::Starting);
    m_tunnel->start(m_server->port());
}

void VehicleSharing::stop()
{
    // Order: no new connections, then end the session (closes the adapter).
    m_tunnel->stop();
    m_server->stop();
    m_secret.clear();
    m_clientName.clear();
    updateCode();
    if (m_state != State::Idle && m_state != State::Failed)
        addLog(QStringLiteral("Sharing stopped"));
    if (m_state != State::Failed)
        setState(State::Idle);
}

void VehicleSharing::allow()
{
    m_server->approve();
}

void VehicleSharing::reject()
{
    m_server->reject();
}

void VehicleSharing::copyCode() const
{
    if (!m_connectionCode.isEmpty())
        QGuiApplication::clipboard()->setText(m_connectionCode);
}

void VehicleSharing::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
}

void VehicleSharing::fail(const QString &message)
{
    // Failed is sticky through stop() so the message stays visible.
    m_state = State::Failed;
    stop();
    m_errorText = message;
    addLog(message);
    emit stateChanged();
}

void VehicleSharing::updateCode()
{
    const QString code = (m_tunnel->state() == CloudflaredTunnel::State::Ready && !m_secret.isEmpty())
        ? CloudflaredTunnel::shortName(m_tunnel->host()) + QLatin1Char('#') + m_secret
        : QString();
    if (code == m_connectionCode)
        return;
    m_connectionCode = code;
    emit stateChanged();
}

void VehicleSharing::addLog(const QString &text)
{
    m_log.append(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss  ")) + text);
    while (m_log.size() > kMaxLogLines)
        m_log.removeFirst();
    emit logChanged();
}
