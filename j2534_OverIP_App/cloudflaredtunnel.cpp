#include "cloudflaredtunnel.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

#include <windows.h>

namespace {

const QString kQuickTunnelSuffix = QStringLiteral(".trycloudflare.com");

} // namespace

CloudflaredTunnel::CloudflaredTunnel(QObject *parent)
    : QObject(parent)
{
    m_startTimer.setSingleShot(true);
    connect(&m_startTimer, &QTimer::timeout, this, [this] {
        fail(QStringLiteral("cloudflared did not open the tunnel within %1 s. Check the internet connection.")
                 .arg(kStartTimeoutMs / 1000));
    });
}

CloudflaredTunnel::~CloudflaredTunnel()
{
    stop();
    if (m_job)
        CloseHandle(static_cast<HANDLE>(m_job));
}

QString CloudflaredTunnel::findExecutable()
{
    const QString bundled = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("cloudflared.exe"));
    if (QFileInfo::exists(bundled))
        return bundled;
    return QStandardPaths::findExecutable(QStringLiteral("cloudflared"));
}

QString CloudflaredTunnel::shortName(const QString &host)
{
    return host.endsWith(kQuickTunnelSuffix, Qt::CaseInsensitive) ? host.chopped(kQuickTunnelSuffix.size()) : host;
}

QString CloudflaredTunnel::parseQuickTunnelHost(const QString &line)
{
    // "|  https://brave-cat-xyz.trycloudflare.com  |"
    static const QRegularExpression re(QStringLiteral("https://([a-z0-9-]+\\.trycloudflare\\.com)"),
                                       QRegularExpression::CaseInsensitiveOption);
    const auto match = re.match(line);
    // api.trycloudflare.com is the endpoint cloudflared asks for a tunnel, not one.
    if (!match.hasMatch() || match.captured(1).compare(QStringLiteral("api.trycloudflare.com"), Qt::CaseInsensitive) == 0)
        return {};
    return match.captured(1).toLower();
}

void CloudflaredTunnel::start(quint16 localPort)
{
    stop();
    m_host.clear();
    m_error.clear();
    m_recentErrors.clear();
    m_buffer.clear();

    const QString exe = findExecutable();
    if (exe.isEmpty()) {
        fail(QStringLiteral("cloudflared.exe was not found next to the App or on PATH. Reinstall with the "
                            "\"Vehicle PC\" component, or install cloudflared."));
        return;
    }

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::readyRead, this, &CloudflaredTunnel::onOutput);
    connect(m_process, &QProcess::finished, this, [this](int exitCode) { onFinished(exitCode); });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            fail(QStringLiteral("Couldn't start cloudflared: %1").arg(m_process->errorString()));
    });
    connect(m_process, &QProcess::started, this, [this] {
        if (!m_job) {
            HANDLE job = CreateJobObjectW(nullptr, nullptr);
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            if (job && SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
                m_job = job;
            else if (job)
                CloseHandle(job);
        }
        HANDLE process = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, DWORD(m_process->processId()));
        if (m_job && process)
            AssignProcessToJobObject(static_cast<HANDLE>(m_job), process);
        if (process)
            CloseHandle(process);
    });

    setState(State::Starting);
    emit logMessage(QStringLiteral("Starting cloudflared (%1)").arg(QDir::toNativeSeparators(exe)));
    m_process->start(exe, {QStringLiteral("tunnel"), QStringLiteral("--no-autoupdate"),
                           QStringLiteral("--url"), QStringLiteral("http://127.0.0.1:%1").arg(localPort)});
    m_startTimer.start(kStartTimeoutMs);
}

void CloudflaredTunnel::stop()
{
    m_startTimer.stop();
    if (m_process) {
        QProcess *process = m_process;
        m_process = nullptr;
        process->disconnect(this);
        process->kill();   // nothing to clean up; Cloudflare drops the quick tunnel
        process->waitForFinished(3000);
        delete process;
    }
    setState(State::Stopped);
}

void CloudflaredTunnel::onOutput()
{
    m_buffer += m_process->readAll();
    qsizetype newline;
    while ((newline = m_buffer.indexOf('\n')) >= 0) {
        const QString line = QString::fromUtf8(m_buffer.left(newline)).trimmed();
        m_buffer.remove(0, newline + 1);
        if (line.isEmpty())
            continue;

        if (line.contains(QStringLiteral(" ERR "))) {
            m_recentErrors.append(line);
            if (m_recentErrors.size() > 3)
                m_recentErrors.removeFirst();
            emit logMessage(QStringLiteral("cloudflared: %1").arg(line));
        }

        if (m_host.isEmpty()) {
            const QString host = parseQuickTunnelHost(line);
            if (!host.isEmpty()) {
                m_host = host;
                emit logMessage(QStringLiteral("Tunnel address: %1").arg(host));
            }
        }
        // The address exists before Cloudflare can route to it; wait for the
        // first registered connection.
        if (m_state == State::Starting && !m_host.isEmpty()
            && line.contains(QStringLiteral("Registered tunnel connection"))) {
            m_startTimer.stop();
            emit logMessage(QStringLiteral("Tunnel is up"));
            setState(State::Ready);
        }
    }
}

void CloudflaredTunnel::onFinished(int exitCode)
{
    QString message = QStringLiteral("cloudflared stopped (exit code %1).").arg(exitCode);
    if (!m_recentErrors.isEmpty())
        message += QLatin1Char('\n') + m_recentErrors.last();
    m_process->deleteLater();
    m_process = nullptr;
    fail(message);
}

void CloudflaredTunnel::fail(const QString &message)
{
    m_startTimer.stop();
    m_error = message;
    emit logMessage(message);
    if (m_process) {
        QProcess *process = m_process;
        m_process = nullptr;
        process->disconnect(this);
        process->kill();
        process->deleteLater();
    }
    setState(State::Failed);
}

void CloudflaredTunnel::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
}
