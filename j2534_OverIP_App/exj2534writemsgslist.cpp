#include "exj2534writemsgslist.h"
#include <QDateTime>

namespace {

QString hexBytes(const QByteArray &data)
{
    return QString::fromLatin1(data.toHex(' '));
}

} // namespace

ExJ2534WriteMsgsList::ExJ2534WriteMsgsList(QObject *parent)
    : QAbstractListModel{parent}
{
    this->m_roles[ID] = "ID";
    this->m_roles[Timestamp] = "Timestamp";
    this->m_roles[Message] = "Message";
    this->m_roles[HasError] = "HasError";
    this->m_roles[ChannelID] = "ChannelID";
    this->m_roles[ProtocolID] = "ProtocolID";
    this->m_roles[TxFlags] = "TxFlags";
    this->m_roles[DataSize] = "DataSize";
    this->m_roles[Data] = "Data";
}

QVariant ExJ2534WriteMsgsList::data(const QModelIndex & index, int role) const
{
    if (index.row() < 0 || index.row() >= this->p_rows.size())return QVariant();
    const auto &row = this->p_rows.at(index.row());

    switch (role)
    {
    case ID:
        // First data byte, like the RX list (CAN ID bytes start the payload).
        return row.msg.data().isEmpty() ? QString() : QString::number(quint8(row.msg.data().at(0)), 16);
    case Timestamp:
        return QDateTime::fromMSecsSinceEpoch(row.timestamp).toString("[hh:mm:ss.zzz]");
    case Message:
        {
            QString text = QStringLiteral("CH %1  proto %2  flags %3  [%4]  %5")
                               .arg(row.channelId)
                               .arg(quint32(row.msg.protocolId()), 0, 16)
                               .arg(quint32(row.msg.txFlags()), 0, 16)
                               .arg(quint32(row.msg.dataSize()))
                               .arg(hexBytes(row.msg.data()));
            if (!row.sent)
                text += QStringLiteral("  -- not sent");
            if (!row.error.isEmpty())
                text += QStringLiteral("  (") + row.error + QStringLiteral(")");
            return text;
        }
    case HasError:
        return !row.sent;
    case ChannelID:
        return row.channelId;
    case ProtocolID:
        return QString::number(quint32(row.msg.protocolId()), 16);
    case TxFlags:
        return QString::number(quint32(row.msg.txFlags()), 16);
    case DataSize:
        return quint32(row.msg.dataSize());
    case Data:
        return hexBytes(row.msg.data());
    }

    return QVariant();
}

void ExJ2534WriteMsgsList::processPipeMessage(PipeMessage msg)
{
    if(!msg.hasWriteMsgs())return;

    const auto &call = msg.writeMsgs();
    const auto &msgs = call.msgs();
    if(msgs.isEmpty())return;

    const qint64 timestamp = qint64(msg.timestampMs()) != 0 ? qint64(msg.timestampMs()) : QDateTime::currentMSecsSinceEpoch();
    const auto &result = call.result();
    const bool failed = call.hasResult() && result.status() != 0;
    // On failure the driver reports how many of the messages it still sent.
    const qsizetype written = failed ? qsizetype(quint32(call.numMsgsWritten())) : msgs.size();

    QString error;
    if (failed)
        error = result.error().isEmpty() ? QStringLiteral("status 0x%1").arg(qint32(result.status()), 0, 16) : result.error();

    this->beginInsertRows(QModelIndex(), this->p_rows.size(), this->p_rows.size() + msgs.size() - 1);
    for (qsizetype i = 0; i < msgs.size(); ++i)
    {
        const bool sent = i < written;
        this->p_rows.append(Row{timestamp, quint32(call.channelId()), msgs.at(i), sent,
                                sent ? QString() : error});
    }
    this->endInsertRows();
}

void ExJ2534WriteMsgsList::removeElementByIndex(qint32 idx)
{
    if (idx < 0 || idx >= this->p_rows.size())return;
    this->beginRemoveRows(QModelIndex(), idx, idx);
    this->p_rows.removeAt(idx);
    this->endRemoveRows();
}
