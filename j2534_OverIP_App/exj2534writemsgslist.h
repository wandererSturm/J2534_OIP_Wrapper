#ifndef EXJ2534WRITEMSGSLIST_H
#define EXJ2534WRITEMSGSLIST_H

#include <QAbstractListModel>
#include <QObject>
#include <QQmlEngine>

#include "types.h"

// One row per message the host passed to PassThruWriteMsgs.
class ExJ2534WriteMsgsList : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ExJ2534WriteMsgsList)
    QML_UNCREATABLE("Should not be created directly in QML. Will have no functionality")

public:
    enum ElemRoles
    {
        ID = Qt::UserRole + 1,
        Timestamp,
        Message,
        HasError,
        ChannelID,
        ProtocolID,
        TxFlags,
        DataSize,
        Data
    };

    explicit ExJ2534WriteMsgsList(QObject *parent = nullptr);

    int rowCount(const QModelIndex & parent = QModelIndex()) const {return this->p_rows.count();}
    QVariant data(const QModelIndex & index, int role = ExJ2534WriteMsgsList::ID) const;
    QHash<int, QByteArray> roleNames() const {return this->m_roles;}

public slots:
    void processPipeMessage(PipeMessage msg);
    void removeElementByIndex(qint32 idx);

private:
    struct Row
    {
        qint64 timestamp;
        quint32 channelId;
        j2534::pipe::PassThruMsg msg;
        bool sent;          // false if the driver reported fewer messages written
        QString error;      // CallResult error or status of the WriteMsgs call
    };

    QHash<int, QByteArray>          m_roles;
    QList<Row>                      p_rows;
};

#endif // EXJ2534WRITEMSGSLIST_H
