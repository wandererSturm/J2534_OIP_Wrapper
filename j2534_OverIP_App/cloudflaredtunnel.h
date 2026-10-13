#ifndef CLOUDFLAREDTUNNEL_H
#define CLOUDFLAREDTUNNEL_H

#include <QObject>
#include <QStringList>
#include <QTimer>

class QProcess;

// Runs `cloudflared tunnel --url http://127.0.0.1:<port>` (a trycloudflare
// quick tunnel: no account, random host name, changes every run) and reports
// the public host once Cloudflare has registered a connection.
class CloudflaredTunnel : public QObject
{
    Q_OBJECT

public:
    enum class State { Stopped, Starting, Ready, Failed };

    explicit CloudflaredTunnel(QObject *parent = nullptr);
    ~CloudflaredTunnel() override;

    // cloudflared.exe next to the App, otherwise from PATH. Empty if neither.
    static QString findExecutable();

    void start(quint16 localPort);
    void stop();

    State state() const { return m_state; }
    // "brave-cat-xyz.trycloudflare.com" once Ready
    QString host() const { return m_host; }
    QString errorString() const { return m_error; }

    // "brave-cat-xyz.trycloudflare.com" -> "brave-cat-xyz", other hosts unchanged.
    static QString shortName(const QString &host);
    // Finds the quick tunnel URL in a cloudflared log line; empty if none.
    static QString parseQuickTunnelHost(const QString &line);

    static constexpr int kStartTimeoutMs = 60000;

signals:
    void stateChanged();
    void logMessage(const QString &text);

private:
    void onOutput();
    void onFinished(int exitCode);
    void setState(State state);
    void fail(const QString &message);

    QProcess *m_process = nullptr;
    QByteArray m_buffer;
    State m_state = State::Stopped;
    QString m_host;
    QString m_error;
    QStringList m_recentErrors;   // last "ERR" lines, for the failure message
    QTimer m_startTimer;
    // Job object that ends cloudflared when the App exits, even if it crashes.
    void *m_job = nullptr;
};

#endif // CLOUDFLAREDTUNNEL_H
