#include "exj2534wrapper.h"
#include "defs.h"
#include "pipecapture.h"
#include <QTimer>
#include <QDateTime>
#include <QGuiApplication>


ExJ2534Wrapper::ExJ2534Wrapper(QObject * parent):QAbstractListModel(parent)
{
    this->m_roles[ID] = "ID";
    this->m_roles[Timestamp] = "Timestamp";
    this->m_roles[Message] = "Message";
    this->m_roles[HasError] = "HasError";

    this->p_read_msgs = new ExJ2534ReadMsgsList(this);
    this->p_write_msgs = new ExJ2534WriteMsgsList(this);

    this->p_loaded_funcs = nullptr;
    this->p_loaded_dll_funcs = new QStringListModel(this);
    this->p_unloaded_dll_funcs = new QStringListModel(this);
}

ExJ2534Wrapper * ExJ2534Wrapper::getInstance()
{
    if(ExJ2534Wrapper::s_ExJ2534Wrapper_singletonInstance == nullptr)ExJ2534Wrapper::s_ExJ2534Wrapper_singletonInstance = new ExJ2534Wrapper(nullptr);

    return ExJ2534Wrapper::s_ExJ2534Wrapper_singletonInstance;
}

ExJ2534Wrapper * ExJ2534Wrapper::create(QQmlEngine *, QJSEngine *engine)
{
    auto instance = ExJ2534Wrapper::getInstance();
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

QStringListModel * ExJ2534Wrapper::loaded_dll_funcs()
{
    this->p_loaded_dll_funcs->setStringList(this->getDllFuncs());
    return this->p_loaded_dll_funcs;
}

QStringListModel * ExJ2534Wrapper::unloaded_dll_funcs()
{
    this->p_unloaded_dll_funcs->setStringList(this->getDllFuncs(false));
    return this->p_unloaded_dll_funcs;
}

QStringList ExJ2534Wrapper::getDllFuncs(bool loaded)
{
    QStringList list;
    /*
    if(loaded)
    {
        list.append("test 1");
        list.append("test 2");
        list.append("test 3");
        list.append("test 4");
        list.append("test 5");
    }*/

    if(this->p_loaded_funcs == nullptr)return list;

    for(const auto &fn: this->p_loaded_funcs->msg.dllLoaded().functions())
    {
        if(fn.available() == loaded)
        {
            list.append(fn.name());
        }
    }

    return list;
}


void ExJ2534Wrapper::processPipeMessage(PipeMessage msg)
{
    if(msg.timestampMs() == 0)msg.setTimestampMs(QDateTime::currentDateTime().toMSecsSinceEpoch());
    //qDbg() << "[TIMESTAMP]: " << msg.timestampMs();

    auto obj = new TimestampedObject{msg.timestampMs(), msg};
    QTimer::singleShot(0,[obj, this]{this->addElement(obj);});

    switch(msg.payloadField())
    {
    case PipePayload::ModuleStarted:
        {
            qDbg() << "[MSG]: J2534DllWrapper constructor: 0x" << QString::number(quint64(msg.moduleStarted().instance()), 16);
        }
        break;
    case PipePayload::DllLoaded:
        {
            const auto &loaded = msg.dllLoaded();
            qDbg() << "[MSG]: " << loaded.path();
            if(!loaded.error().isEmpty()){
                obj->hasError = true;
                qDbg() << "[MSG] loadDll failed: " << loaded.error();
            }
            this->p_loaded_funcs = obj;
            emit dll_funcsChanged();
            for(const auto &fn: loaded.functions())
            {
                qDbg() << "[MAP] " << fn.name() << " loaded: " << fn.available();
            }
        }
        break;
    case PipePayload::ReadMsgs:
        {
            this->p_read_msgs->processPipeMessage(msg);
        }
        break;
    case PipePayload::WriteMsgs:
        {
            this->p_write_msgs->processPipeMessage(msg);
        }
        break;
    case PipePayload::QueueMsgs:
        {
            qDbg() << "[QUEUE_MSG] ";
        }
        break;
    default:
        break;
    }
}

void ExJ2534Wrapper::exportToJSON()
{
    QList<PipeMessage> messages;
    for(auto it: this->p_objs)messages.append(it->msg);

    auto path = QGuiApplication::instance()->applicationDirPath().append(QStringLiteral("\\export.json"));
    QString error;
    if(!PipeCapture::save(path, messages, &error))
    {
        qWarning() << "[ExJ2534Wrapper] export failed:" << error;
    }
}

bool ExJ2534Wrapper::loadFromJSON(QString fpath)
{
    QString path;
    if(!fpath.isEmpty())path = fpath;
    else path = QGuiApplication::instance()->applicationDirPath().append(QStringLiteral("\\export.json"));

    QList<PipeMessage> messages;
    QString error;
    if(!PipeCapture::load(path, &messages, &error))
    {
        qWarning() << "[ExJ2534Wrapper] load failed:" << error;
        return false;
    }

    for(const auto &pmsg: messages)this->processPipeMessage(pmsg);
    return true;
}

void ExJ2534Wrapper::addElement(TimestampedObject * obj)
{
    //qDbg() << "[addElement]: " << obj->msg.message;
    this->beginInsertRows(QModelIndex(), 0, 0);
    this->p_objs.insert(0, obj);
    this->endInsertRows();
}

void ExJ2534Wrapper::removeElement(TimestampedObject * obj)
{
    auto idx = this->p_objs.indexOf(obj);
    this->beginRemoveRows(QModelIndex(), idx, idx);
    this->p_objs.removeOne(obj);
    this->endRemoveRows();
}

void ExJ2534Wrapper::removeElementByIndex(qint32 idx)
{
    auto obj = this->p_objs.at(idx);
    this->beginRemoveRows(QModelIndex(), idx, idx);
    this->p_objs.removeOne(obj);
    this->endRemoveRows();
}


QVariant ExJ2534Wrapper::data(const QModelIndex & index, int role) const
{
    if (index.row() < 0 || index.row() > this->p_objs.size())return QVariant();

    auto obj = this->p_objs[index.row()];

    switch (role) {
    case ID:
        return quint32(obj->msg.clientId());
    case Timestamp:
        return QDateTime::fromMSecsSinceEpoch(obj->timestamp).toString("[hh:mm:ss.zzz]");
    case Message:
        return obj->msg.text();
    case HasError:
        return obj->hasError;
    }

    return QVariant();
}
