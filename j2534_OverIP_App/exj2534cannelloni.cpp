#include "exj2534cannelloni.h"
#include "defs.h"
#include "pipecapture.h"
#include <QTimer>
#include <QDateTime>
#include <QGuiApplication>
#include <QDir>
#include <QSettings>

ExJ2534Cannelloni::ExJ2534Cannelloni(QObject * parent):QAbstractListModel(parent)
{
    this->m_roles[ID] = "ID";
    this->m_roles[Timestamp] = "Timestamp";
    this->m_roles[Message] = "Message";
    this->m_roles[DATA] = "DATA";

    this->p_read_msgs = new ExJ2534ReadMsgsList(this);
    this->p_write_msgs = new ExJ2534WriteMsgsList(this);
}

ExJ2534Cannelloni * ExJ2534Cannelloni::getInstance()
{
    if(ExJ2534Cannelloni::s_ExJ2534Cannelloni_singletonInstance == nullptr)ExJ2534Cannelloni::s_ExJ2534Cannelloni_singletonInstance = new ExJ2534Cannelloni(nullptr);

    return ExJ2534Cannelloni::s_ExJ2534Cannelloni_singletonInstance;
}

ExJ2534Cannelloni * ExJ2534Cannelloni::create(QQmlEngine *, QJSEngine *engine)
{
    auto instance = ExJ2534Cannelloni::getInstance();
    // The instance has to exist before it is used. We cannot replace it.
    Q_ASSERT(instance);

    // The engine has to have the same thread affinity as the singleton.
    Q_ASSERT(engine->thread() == instance->thread());

    // There can only be one engine accessing the singleton.
    if (s_engine)
        Q_ASSERT(engine == s_engine);
    else
        s_engine = engine;

    // Explicitly specify C++ ownership so that the engine doesn't delete
    // the instance.
    QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
    return instance;
}

void ExJ2534Cannelloni::processPipeMessage(PipeMessage msg)
{
    if(msg.timestampMs() == 0)msg.setTimestampMs(QDateTime::currentDateTime().toMSecsSinceEpoch());
    auto obj = new TimestampedObject{msg.timestampMs(), msg};

    bool proceeded = false;

    qDbg() << "[Payload] " << msg.payloadField();

    switch(msg.payloadField())
    {
    case PipePayload::Open:
        {
            proceeded = true;
            const auto &open = msg.open();
            obj->data = QString("pName = ") + open.name() + " pDeviceID = " + QString::number(quint32(open.deviceId()));
            if(!open.result().error().isEmpty())
            {
                obj->hasError = true;
                obj->data += " error = " + open.result().error();
            }
            qDbg() << "[OPEN] " << obj->data;
            QTimer::singleShot(0,[this, obj]{this->addElement(obj);});
        }
        break;
    case PipePayload::ReadMsgs:
        proceeded = true;
        this->p_read_msgs->processPipeMessage(msg);
        break;
    case PipePayload::WriteMsgs:
        proceeded = true;
        this->p_write_msgs->processPipeMessage(msg);
        break;
    default:
        break;
    }

    if(!proceeded)
    {
        qDbg() << "[ExJ2534Cannelloni] MSG NOT PROCEEDED! payload " << msg.payloadField() << msg.text();
        QTimer::singleShot(0,[this, obj]{this->addElement(obj);});
    }
}


QString ExJ2534Cannelloni::exportToJSON()
{
    QList<PipeMessage> messages;
    for(auto it: this->p_objs)messages.append(it->msg);

    auto path = QGuiApplication::instance()->applicationDirPath().append(QStringLiteral("\\export.json"));
    QString error;
    if(!PipeCapture::save(path, messages, &error))
    {
        qWarning() << "[ExJ2534Cannelloni] export failed:" << error;
        return {};
    }
    return QDir::toNativeSeparators(path);
}

// Same key and defaults as J2534CannelloniClient::getDefaults() in the DLL.
QString ExJ2534Cannelloni::settingsLocation() const
{
    return QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\PassThruSupport.04.04\\XplatformsPassThruOverIP");
}

bool ExJ2534Cannelloni::settingsFound() const
{
    QSettings s(settingsLocation(), QSettings::NativeFormat);
    return s.contains(QStringLiteral("DefaultHost")) || s.contains(QStringLiteral("DefaultPort"))
        || s.contains(QStringLiteral("CannelloniDataVersion"));
}

QString ExJ2534Cannelloni::defaultHost() const
{
    QSettings s(settingsLocation(), QSettings::NativeFormat);
    return s.value(QStringLiteral("DefaultHost"), QStringLiteral("127.0.0.1")).toString();
}

int ExJ2534Cannelloni::defaultPort() const
{
    QSettings s(settingsLocation(), QSettings::NativeFormat);
    return s.value(QStringLiteral("DefaultPort"), 20000).toInt();
}

int ExJ2534Cannelloni::dataVersion() const
{
    QSettings s(settingsLocation(), QSettings::NativeFormat);
    return s.value(QStringLiteral("CannelloniDataVersion"), 1).toInt();
}

bool ExJ2534Cannelloni::loadFromJSON(QString fpath)
{
    QString path;
    if(!fpath.isEmpty())path = fpath;
    else path = QGuiApplication::instance()->applicationDirPath().append(QStringLiteral("\\export.json"));

    QList<PipeMessage> messages;
    QString error;
    if(!PipeCapture::load(path, &messages, &error))
    {
        qWarning() << "[ExJ2534Cannelloni] load failed:" << error;
        return false;
    }

    for(const auto &pmsg: messages)this->processPipeMessage(pmsg);
    return true;
}

void ExJ2534Cannelloni::addElement(TimestampedObject * obj)
{
    this->beginInsertRows(QModelIndex(), 0, 0);
    this->p_objs.insert(0, obj);
    this->endInsertRows();
}

void ExJ2534Cannelloni::removeElement(TimestampedObject * obj)
{
    auto idx = this->p_objs.indexOf(obj);
    this->beginRemoveRows(QModelIndex(), idx, idx);
    this->p_objs.removeOne(obj);
    this->endRemoveRows();
}

void ExJ2534Cannelloni::removeElementByIndex(qint32 idx)
{
    auto obj = this->p_objs.at(idx);
    this->beginRemoveRows(QModelIndex(), idx, idx);
    this->p_objs.removeOne(obj);
    this->endRemoveRows();
}

QVariant ExJ2534Cannelloni::data(const QModelIndex & index, int role) const
{
    if (index.row() < 0 || index.row() > this->p_objs.size())return QVariant();

    auto obj = this->p_objs[index.row()];

    switch (role) {
    case ID:
        return quint32(obj->msg.clientId());
    case Timestamp:
        return QDateTime::fromMSecsSinceEpoch(obj->timestamp).toString("hh:mm:ss.zzz");
    case Message:
        return obj->msg.text();
    case DATA:
        return obj->data;
    }

    return QVariant();
}
