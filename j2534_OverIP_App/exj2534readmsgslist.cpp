#include "defs.h"
#include "exj2534readmsgslist.h"
#include "types.h"

#include <QDateTime>

#include <algorithm>
#include <cstring>

ExJ2534ReadMsgsList::ExJ2534ReadMsgsList(QObject *parent)
    : QAbstractListModel{parent}
{
    this->m_roles[ID] = "ID";
    this->m_roles[HasError] = "HasError";
    this->m_roles[ProtocolID] = "ProtocolID";
    this->m_roles[RxStatus] = "RxStatus";
    this->m_roles[TxFlags] = "TxFlags";
    this->m_roles[Timestamp] = "Timestamp";
    this->m_roles[DataSize] = "DataSize";
    this->m_roles[ExtraDataIndex] = "ExtraDataIndex";
    this->m_roles[Data] = "Data";
}

QVariant ExJ2534ReadMsgsList::data(const QModelIndex & index, int role) const
{
    //qDbg() << "[RX_GET_DATA] index " << index << " role " << role << " objs_count " << this->p_objs.count();

    if (index.row() < 0 || index.row() > this->p_objs.size())return QVariant();
    auto obj = this->p_objs[index.row()];
    //qDbg() << "[OBJ] " << obj->ProtocolID << obj->RxStatus << obj->DataSize << obj->Data;

    switch (role)
    {
    case ID:
        return QString::number(obj->Data[0], 16);
    case ProtocolID:
        //qDbg() << "[ProtocolID] " << obj->ProtocolID;
        //qDbg() << "[ProtocolID2] " << QString::number(obj->ProtocolID, 16);
        return QString::number(obj->ProtocolID, 16);
    case RxStatus:
        //qDbg() << "[RxStatus] " << obj->RxStatus;
        //qDbg() << "[RxStatus2] " << QString::number(obj->RxStatus, 16);
        return QString::number(obj->RxStatus, 16);
    case TxFlags:
        return QString::number(obj->TxFlags, 16);
    case Timestamp:
        return QDateTime::fromMSecsSinceEpoch(obj->Timestamp).toString("[hh:mm:ss.zzz]");
    case DataSize:
        return QVariant((quint32)obj->DataSize);
    case ExtraDataIndex:
        return QVariant((quint32)obj->ExtraDataIndex);
    case Data:
        {
            QString hex;
            for(int y = 0; y < obj->DataSize; y++)
            {
                hex.append(QLatin1String(" ") + QString::number(obj->Data[y], 16));
                //printf("%02x ", obj->Data[y]);
            }
            return hex;
        }
    case HasError:
        return false;
    }

    return QVariant();
}


void ExJ2534ReadMsgsList::processPipeMessage(PipeMessage msg)
{
    if(!msg.hasReadMsgs())return;

    const auto &msgs = msg.readMsgs().msgs();
    if(msgs.isEmpty())
    {
        qDbg() << "[READ_MSG]: no messages returned";
        return;
    }

    for(const auto &src: msgs)
    {
        PASSTHRU_MSG * pmsg = new PASSTHRU_MSG();
        memset(pmsg, 0, sizeof(PASSTHRU_MSG));
        pmsg->ProtocolID = src.protocolId();
        pmsg->RxStatus = src.rxStatus();
        pmsg->TxFlags = src.txFlags();
        pmsg->Timestamp = src.timestamp();
        pmsg->DataSize = src.dataSize();
        pmsg->ExtraDataIndex = src.extraDataIndex();

        const auto &data = src.data();
        if(qsizetype(pmsg->DataSize) != data.size())
        {
            qDbg() << "[CAN_RX_MSG] data length != DataSize in PASSTHRU_MSG!";
        }
        memcpy(pmsg->Data, data.constData(), std::min<size_t>(data.size(), sizeof(pmsg->Data)));

        this->addElement(pmsg);
    }
}

void ExJ2534ReadMsgsList::addElement(PASSTHRU_MSG * obj)
{
    qDbg() << "[ADD_RX_ELEMENT] count " << this->p_objs.count();
    this->beginInsertRows(QModelIndex(), this->rowCount(), this->rowCount());
    this->p_objs.append(obj);
    this->endInsertRows();
}

void ExJ2534ReadMsgsList::removeElement(PASSTHRU_MSG * obj)
{
    auto idx = this->p_objs.indexOf(obj);
    this->beginRemoveRows(QModelIndex(), idx, idx);
    this->p_objs.removeOne(obj);
    this->endRemoveRows();
}

void ExJ2534ReadMsgsList::removeElementByIndex(qint32 idx)
{
    auto obj = this->p_objs.at(idx);
    this->beginRemoveRows(QModelIndex(), idx, idx);
    this->p_objs.removeOne(obj);
    this->endRemoveRows();
}
